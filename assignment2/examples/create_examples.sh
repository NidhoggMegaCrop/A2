#!/bin/bash

umask 0022

# create some variables
d=$(readlink -f .)
examples=$d/examples

# FIXME change to using C reference implementation
PATH=$d:$d/solutions:$PATH
reference_implementation=/web/cs1521/bin/forest

which $reference_implementation || $(echo "no $reference_implementation referance found" >&2 && exit 1)

mkdir -m 755 -p $examples
temp_dir=$(mktemp -d)
trap 'cd /;rm -fr $temp_dir' EXIT
cd $temp_dir

# Git doesn't save perms, so we need to set them
chmod 755 $examples/.hello_world.d/hello.{pl,py,sh}

# work around .gitignore rules
# move already existing files to usefull places
for i in $examples/.*; do
    # skip . and ..
    case $(basename "${i}") in
        .|..) continue;;
    esac

    file=$(basename "${i}")
    rm -rf "${examples}"/"${file:1}"
    cp -r "${i}" "${examples}"/"${file:1}"
done

# create some files we will be using
cat > hello.txt <<eof
Hello COMP1521
I hope you are enjoying this assignment.
eof

cat > last_goodbye.txt <<eof
This is our last goodbye
I hate to feel the love between us die
But it's over
Just hear this and then I'll go
You gave me more to live for
More than you'll ever know
eof

cat > these_days.txt <<eof
these days turned out nothing like i had planned
control well it's slipping right through my hands
these days turned out nothing like i had planned
eof

seq 0 255 | xargs -i@ printf "%02x" @ | xxd -r -p > 256.bin

chmod 644 *.txt *.bin

# pack the files into groups
function create_tree_template() {
    tree_groups_name="$1"
    shift
    rm    -rf    "${examples}"/"${tree_groups_name}.d"/
    mkdir -p     "${examples}"/"${tree_groups_name}.d"/
    cp    -r  $@ "${examples}"/"${tree_groups_name}.d"/
}

function mod_tree_template_file() {
    tree_groups_name="$1"
    tree_file_name="$2"
    tree_file_mode="$3"
    chmod ${tree_file_mode} "${examples}"/"${tree_groups_name}.d"/"${tree_file_name}"
}

create_tree_template   text_file   hello.txt

create_tree_template   binary_file 256.bin

create_tree_template   2_files     hello.txt last_goodbye.txt

create_tree_template   3_files     hello.txt last_goodbye.txt these_days.txt
mod_tree_template_file 3_files     last_goodbye.txt 604

create_tree_template   4_files     hello.txt last_goodbye.txt these_days.txt 256.bin
mod_tree_template_file 4_files     last_goodbye.txt 404
mod_tree_template_file 4_files     these_days.txt   464

# create some simple directory based trees
function create_directory_tree_template() {
    tree_groups_name="$1"
    shift
    rm    -rf "${examples}"/"${tree_groups_name}.d"/
    mkdir -p  "${examples}"/"${tree_groups_name}.d"/
    for file in $@; do
        dir=$(dirname  "${file}")
        fil=$(basename "${file}")
        mkdir -p          "${examples}"/"${tree_groups_name}.d"/"${dir}"/
        cp    -r "${fil}" "${examples}"/"${tree_groups_name}.d"/"${dir}"/
    done
}

function mod_directory_tree_template_file() {
    tree_groups_name="$1"
    tree_file_name="$2"
    dir=$(dirname  "${tree_file_name}")
    fil=$(basename "${tree_file_name}")
    tree_file_mode="$3"
    chmod ${tree_file_mode} "${examples}"/"${tree_groups_name}.d"/"${dir}"/"${fil}"
}

create_directory_tree_template   1_file.subdirectory hello/hello.txt

create_directory_tree_template   3_files.subdirectory these_days.txt hello/hello.txt goodbye/last_goodbye.txt
mod_directory_tree_template_file 3_files.subdirectory goodbye/last_goodbye.txt 444

# create some complex directory based trees

j='.'
for i in 1 2 3 4 5 6 7
do
    j=$j/dir$i/
    mkdir $j
    chmod 7$i$i $j
    for k in $(seq 1 $i)
    do
        echo "This is test file $k at level $i" > $j/file$k
        chmod 6$i$i $j/file$k
    done
done

create_tree_template all_the_modes.subdirectory dir1

# create one tree file for each possible format
function create_tree_from_template() {
    tree_template_name="$1"
    cd "${tree_template_name}"
    chmod -R ugo+r * # can't add a file that isn't user readable, and autotest needs other readable
    chmod ugo+x $(find . -type d)
    for format in 8 7 6
    do
        case "$format" in
        8)
            tree="$(basename ${tree_template_name%.d}).tree"
            format_arg=''
            ;;
        *)
            tree="$(basename ${tree_template_name%.d}).${format}-bit.tree"
            format_arg=-$format
        esac
        if $reference_implementation $format_arg --create $tree $(echo * | sort) >/dev/null 2>&1
        then
            mv  -f $tree ../
        else
            rm $tree
        fi
    done
    cd $temp_dir
}

for i in $examples/*.d; do
    create_tree_from_template "$i"
done

# create some tree files with invalid magic numbers
# only breaks the first leaf
# FIXME break more trees
function break_tree_magic() {
    tree_name="$1"
    cp "${examples}"/"${tree_name}" "${examples}"/"$(basename ${tree_name%.tree}).bad_magic.tree"
    magic=$(dd status=none bs=1 skip=0 count=1 if="${examples}"/"${tree_name}")
    # seed="$magic"
    # while [ "$magic" = "$seed" ]; do seed=$(dd status=none bs=1 count=1 if="/dev/urandom"); done
    seed=$(($(printf '%d' "'$magic") ^ 85 ^ 15)) # xor twice to we can never get 0
    printf '%02x' $seed|
    xxd -r -p|
    dd status=none conv=nocreat,notrunc bs=1 seek=0 count=1 of="${examples}"/"$(basename ${tree_name%.tree}).bad_magic.tree"

    if cmp "${examples}"/"${tree_name}" "${examples}"/"$(basename ${tree_name%.tree}).bad_magic.tree" >/dev/null
    then
        echo breaking "${examples}"/"${tree_name}" failed 1>&2
        exit 1
    fi
}

break_tree_magic text_file.tree
break_tree_magic 3_files.tree
break_tree_magic hello_world.tree
break_tree_magic 3_files.subdirectory.tree

# create some tree files with invalid hashes
# only breaks the last leaf
# FIXME break more trees
function break_tree_hash() {
    tree_name="$1"
    cp "${examples}"/"${tree_name}" "${examples}"/"$(basename ${tree_name%.tree}).bad_hash.tree"
    n_bytes=$(stat -c '%s' "${examples}"/"${tree_name}")
    magic=$(dd status=none bs=1 skip=$((n_bytes - 1)) count=1 if="${examples}"/"${tree_name}")
    # seed="$magic"
    # while [ "$magic" = "$seed" ]; do seed=$(dd status=none bs=1 count=1 if="/dev/urandom"); done
    seed=$(($(printf '%d' "'$magic") ^ 85 ^ 15)) # xor twice to we can never get 0

    printf '%02x' $seed|
    xxd -r -p|
    dd status=none conv=nocreat,notrunc bs=1 seek=$((n_bytes - 1)) count=1 of="${examples}"/"$(basename ${tree_name%.tree}).bad_hash.tree"

    if cmp "${examples}"/"${tree_name}" "${examples}"/"$(basename ${tree_name%.tree}).bad_hash.tree" >/dev/null
    then
        echo breaking "${examples}"/"${tree_name}" failed 1>&2
        exit 1
    fi
}

break_tree_hash text_file.tree
break_tree_hash 3_files.tree
break_tree_hash hello_world.tree
break_tree_hash 3_files.subdirectory.tree

# create trees with multiple formats
cd "${examples}"/4_files.d
$reference_implementation -7 -c ../all_three_formats.6-bit.tree hello.txt >/dev/null
$reference_implementation -6 -a ../all_three_formats.6-bit.tree these_days.txt >/dev/null
$reference_implementation -7 -a ../all_three_formats.6-bit.tree last_goodbye.txt >/dev/null
$reference_implementation -8 -a ../all_three_formats.6-bit.tree 256.bin >/dev/null

# create trees of trees
mkdir -p "${examples}"/meta.d/
cp "${examples}"/*.tree "${examples}"/meta.d/
create_tree_from_template "${examples}"/meta.d
