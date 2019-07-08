#include <stdio.h>
#include <stdlib.h>
#include <sys/root.h>
#include <assert.h>

#include "pan_sys.h"

#include "pan_system.h"
#include "pan_error.h"
#include "pan_malloc.h"

#include "pan_comm.h"
#include "pan_comm_inf.h"
#include "pan_bcst_seq.h"
#include "pan_bcst_fwd.h"
#include "pan_bcst_snd.h"
#include "pan_ucast.h"
#include "pan_msg_cntr.h"






/*---- for monitoring and statistics -----------------------------------------*/

static pan_msg_counter_p info_counter[COUNTER_ITEMS];	/* to retrieve info */

info_t              pan_comm_info_data;

static info_t      *collected_info; /* sequencer collects all info_data */


static char         slot_name[MEMBER_ITEMS][32] = {
			"Total heap space",
			"Leaked heap space",
			"MC Max congestion count",
			"MC Discarded (double arrival)",
			"MC Send pb/direct msg",
			"MC Send pb/express msg",
			"MC Send gsb/express msg",
			"MC Send meta msg",
			"Total time",
			"MC Orderer behind",
			"UC fragments sent",
			"UC smalls sent",
			"UC fragments sent queued",
			"UC smalls sent queued",
			"UC fragments rcvd",
			"UC smalls rcvd",
			"UC daemon behind"
			};

static char         on_mem_name[SEQ_MEMBER_ITEMS][32] = {
			"Ack pb/direct",
			"Ack pb/indirect",
			"Ack gsb"
			};

static char         seq_slot_name[SEQ_ITEMS][15] = {
			"PB count",
			"seq threads",
			};





void
pan_comm_info_register_counter(int n, pan_msg_counter_p counter)
{
    info_counter[n - MEMBER_ITEMS] = counter;
}


void
pan_comm_info_seq_put(int *pb_direct, int *pb_indirect, int *gsb)
{
    int i;

    for (i = 0; i < pan_sys_total_platforms; i++) {
	collected_info[i][PB_DIRECT_SLOT] = pb_direct[i];
    }
    for (i = 0; i < pan_sys_total_platforms; i++) {
	collected_info[i][PB_INDIRECT_SLOT] = pb_indirect[i];
    }
    for (i = 0; i < pan_sys_total_platforms; i++) {
	collected_info[i][GSB_SLOT] = gsb[i];
    }
}


static void
master_exchange_info(void)
{
    int       i;
    int       j;
    LinkCB_t *info_link;
    int       error;
    int       size;

    pan_comm_info_data[LOST_SLOT] -= pan_mallsize(collected_info) / 1024;

    size = INFO_ITEMS * sizeof(int);
    for (i = 0; i < pan_sys_total_platforms; i++) {
	if (i == pan_sys_Parix_id) {
	    for (j = 0; j < INFO_ITEMS; j++)
		collected_info[i][j] = ((int*)pan_comm_info_data)[j];
	} else {
	    info_link = MakeLink(i, INFO_M, &error);
	    if (RecvLink(info_link, collected_info[i], size) != size) {
		pan_panic("RecvLink/info failed\n");
	    }
	    BreakLink(info_link);
	}
    }
}



static void
slave_exchange_info(void)
{
    LinkCB_t *info_link;
    int       error;
    int       size;

    size = INFO_ITEMS * sizeof(int);
    info_link = GetLink(pan_sys_sequencer, INFO_M, &error);
    if (SendLink(info_link, pan_comm_info_data, size) != size) {
	pan_panic("SendLink/info failed\n");
    }
    BreakLink(info_link);
}



static void
pan_comm_print_info(FILE *out)
{
    int         i, j, k;

    fprintf(out, "\n");
					/* print collected statistics */
    for (k = 0, j = 0; j < MEMBER_ITEMS; k++, j++) {
	fprintf(out, " %s:\n", slot_name[k]);
	for (i = 0; i < pan_sys_total_platforms; i++) {
	    fprintf(out, " %8d ", collected_info[i][j]);
	    if ((i + 1) % pan_sys_DimX == 0)
		fprintf(out, "\n");
	}
    }

					/* print counter statistics */
    for (k = 0; j < INFO_ITEMS; k++, j += 3) {
	fprintf(out, " Usage %10s:\n", info_counter[k]->name);
	for (i = 0; i < pan_sys_total_platforms; i++) {
	    fprintf(out, " %2d %2d %2d|",
			 collected_info[i][j],
			 collected_info[i][j + 1],
			 collected_info[i][j + 2]);
	    if ((i + 1) % pan_sys_DimX == 0)
		fprintf(out, "\n");
	}
    }

					/* print on-sequencer statistics */
    for (k = 0; j < ON_MEMBER_ITEMS; k++, j++) {
	fprintf(out, " %s:\n", on_mem_name[k]);
	for (i = 0; i < pan_sys_total_platforms; i++) {
	    fprintf(out, " %8d ", collected_info[i][j]);
	    if ((i + 1) % pan_sys_DimX == 0)
		fprintf(out, "\n");
	}
    }

					/* print sequencer statistics */
    for (k = 0; j < TOTAL_ITEMS; k++, j++) {
	fprintf(out, " %s:", seq_slot_name[k]);
	fprintf(out, " %8d\n", collected_info[pan_sys_sequencer][j]);
    }
}



void
pan_comm_info(char *str1, char *str2, char *str3, char *str4)
{
    mallinfo_t  usage;
    int         i, j;
    FILE       *out_device;

    pan_comm_bcast_fwd_info();
    pan_comm_bcast_snd_info();
    pan_comm_ucast_info();

			/* Marshall the counter statistics into info_data */
    j = MEMBER_ITEMS;
    for (i = 0; i < COUNTER_ITEMS - 1; i++) {
	pan_comm_info_data[j++] = info_counter[i]->total_nr;
	pan_comm_info_data[j++] = info_counter[i]->total_nr -
					info_counter[i]->sema.Count;
	pan_comm_info_data[j++] = info_counter[i]->total_nr -
					info_counter[i]->max_used;
    }

    usage = mallinfo();
    pan_comm_info_data[LOST_SLOT] = pan_comm_info_data[MEM_SLOT] -
					usage.freearena / 1024;

    if (pan_sys_Parix_id == pan_sys_sequencer) {

	pan_comm_bcast_seq_info();

	master_exchange_info();

	if (pan_sys_verbose_level & 1)
	    pan_comm_print_info(stdout);

	if (pan_sys_verbose_level & 2) {
	    if ((out_device = fopen("log", "a")) == NULL) {
		pan_sys_printf("FAIL: to open LOG-FILE\n");
		return;
	    }
	    fprintf(out_device, "\n%s # %s, %s, %s\n", str1, str2, str3, str4);
	    pan_comm_print_info(out_device);
	    if (fclose(out_device) == 0) {
		printf("LOG WRITTEN\n");
	    }
	}

    } else {
	slave_exchange_info();
    }
}



void
pan_comm_info_start(void)
{
    collected_info = pan_calloc(pan_sys_total_platforms, sizeof(info_t));
}



void
pan_comm_info_end(void)
{
    pan_free(collected_info);
    collected_info = NULL;
}
