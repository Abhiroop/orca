#! /usr/local/bin/perl
#
# Author: Tim Ruhl
#
# Create a source directory with symbolic links to all source files
# specified as arguments. If the makefile (first argument) is changed,
# then make new links to all files. All arguments must have absolute
# path names.
#
# Usage: update.pl <makefile> <source files>*


# global($makefile, @files);

($makefile, @files) = @ARGV;


# Update the copy of the file given as argument if the argument file is
# newer.
sub update
{
    local($file) = @_; # Arguments
    local($makecopy, @stat_src, @stat_dest);
    local($time_src, $time_dest);
    local($split, $dest);

    die "Not an absolute pathname: $file" if $file[0] != '/';

    $split = rindex($file, "/");
    $dest = substr($file, $split + 1);
 
    @stat_dest = stat($dest);
    @stat_src = stat("$file");
    die "Cannot stat source file $file" if !@stat_src;

    $makecopy = 0;
    if (!@stat_dest) {
	#print "Doesn't exist\n";
	$makecopy = 1;
    }else{
	$time_src = $stat_src[9];
	$time_dest = $stat_dest[9];

	if ($time_src > $time_dest){
	    unlink($dest);
	    $makecopy = 1;
	}
    }


    if ($makecopy){
	system "ln -s $system/$file $dest";
	#system "cp $system/$file $dest";
	system "chmod g+rw $dest";
    }

    # Return whether we made a copy or not.
    $makecopy;
}


# Update all remaining arguments
sub update_all
{
    local($split, $dest);
    
    foreach (@files){
	$split = rindex($_, "/");
	$dest = substr($_, $split + 1);
	
	system "ln -s $_ $dest";
	#system "cp $_ $dest";
	system "chmod g+rw $dest";
    }
}

# Main

if (&update($makefile)){
    # Makefile changed, update all copies
    &update_all;
}else{
    foreach(@files){
	&update($_);
    }
}
