	.global	_LABEL
	.global	_PAN_XXX
	.global	_pan_small_enqueue
	.align	4
_LABEL:
	ba _pan_small_enqueue
	mov XXX,%o4

#ifndef CMOST
	.size	_LABEL,.-_LABEL
#endif
_PAN_XXX:				



