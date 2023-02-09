#!/bin/sh

size=5
smooth=false

is_a_number()
{
	input=$1

	if test -z "$input"; then
		return 1
	fi

	if test "$input" -eq "$input" 2>/dev/null; then
		return 0
	fi

	return 1
}

if test -n "$1"; then
	if ! is_a_number "$1" || test "$1" -lt 5; then
		printf "generate_brush.sh: %s is not a valid brush size\n" "$1" >&2
		exit 1
	fi
	size=$1
fi

if test "$2" = "smooth"; then
	smooth=true
fi


echo "#define BRUSH_SIZE $((size*2+1))"
echo "const unsigned char brush_data[BRUSH_SIZE][BRUSH_SIZE] = {"
#shellcheck disable=SC2086
for y in $(seq -${size} ${size}); do
	printf "\t{"
	for x in $(seq -${size} ${size}); do
		res=$(echo "sqrt($x * $x + $y * $y)" | bc -l)

		if test "$(echo "$res < $size" | bc -l)" -eq 1; then
			if $smooth; then
				alpha=$(echo "255 - ($res * 255) / $size" | bc)
				printf "0x%02x, " $alpha
			else
				printf "0xff, "
			fi
		else
			printf "0x00, "
		fi
	done
	echo "},"
done
echo "};"
