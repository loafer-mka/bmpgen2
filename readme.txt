
            1. Foreword

The program was written to generate bitmap images of geographical maps used as illustrative material in 
publications. To build a map you need: a) .arc data files with printed contour lines; b) .rep data files 
of printed reference points (names of settlements, mountains, etc.); c) a file .dat of target marks 
with information about additional marked points printed on the map. Data files of contour lines and
reference points may cover a big area overlapping one displayed on the map. Selection the area displayed 
is carried out on the content of the .dat file of target marks: the program determines the required size 
of the area, taking into account the fields and proportions of the bitmap image, then it selects
grid spacing and an image is formed that includes all of the indicated marks.

Note 1: the program was developed in the 90's for Windows 3.10, then, approximately in 1997, modified
for Windows 95. At the time of development, in fact, there were no sources of public spatial data. To draw
contour liness, reference points, etc. the simplest text formats were used, allowing minimal effort to add
data from any found source. Later in 2019 adapted to work in modern 64-bit Windows, but this upgrade, 
however, is minimal and support for modern geodata formats is not implemented at present.

Note 2: partial automation mode is supported, allowing creation a series of images for one set of contour 
lines and reference points and different target mark sets. To do this, specify a list of files with contour 
lines, with reference points, specify the name of the bitmap to be created, as well as specify the name of 
a nonexistent target mark file and select checkbox "wait for input and ...". In this case bmpgen2 goes into 
standby mode and, as soon as it detects existing mark file, it creates an bitmap image and deletes the input
mark file.
The mark file is used to synchronize the execution of the some control program with bmpgen2; it is recommended 
to create target mark file under any temporary name, then rename it according to how indicated for bmpgen2 and 
wait for its removal. When bmpgen2 is deleting target mark file, the bitmap file has already been created and 
can be renamed or moved to the desired folder.


            2. Latitude and Longitude Format

    01.2345678     angle in degrees as a real number with a point or a comma
    01.23          angle in degrees and minutes (exactly two digits after a period or a comma), 
                   corresponds to 01.3833333 (3) in the previous example
    01°23'45.678"  angle in degrees, minutes and seconds; degrees — mandatory, minutes, and seconds — may be
                   omitted; if all parts are present, then first degrees, then minutes, then seconds. Spaces
                   are not allowed.

All input files are text, can be in OEM encoding (in Cyrillic versions of Windows corresponds to code page 866), 
in ANSI encoding (1251, respectively) and in UTF8 with or without BOM (Byte-Order-Mark).

            3. Countour lines file (.arc)

Example:
    Test Image
         2
         5 ^1 rectangle
      -3.000000       -3.000000
      -3.000000        3.000000
       3.000000        3.000000
       3.000000       -3.000000
      -3.000000       -3.000000
         7 ^2 polyline
      -2.000000       -3.500000
       2.000000       -3.500000
       4.000000        0.000000
       2.000000        3.500000
      -2.000000        3.500000
      -4.000000        0.000000
      -2.000000       -3.500000

The first line of the file contains the name for the entire file ("Test Image" in sample above, unused now).
The second line is the number of contour liness ("2" in sample above).
Further, for each contour is a block:
- The first line of the block is the number of points, an optional line style, and an optional contour name (unused).
- Further, the block contains latitude and longitude of each point of the contour.

The line style is indicated by a number after the ^ symbol:
^0  - white solid line 0.1 mm (will be visible only if superimposed over black)
^1  - black solid line 0.1 mm
^2  - black solid line 0.2 mm
^3  - black solid line 0.3 mm
^4  - black solid line 0.5 mm
^5  - black solid line 0.7 mm
^6  - black solid line 1.0 mm
^7  - black solid line 1.5 mm
^8  - black solid line 2.0 mm
^9  - thin black dot-dot-dot line
^10 - thin black dash-dash-dash line
^11 - thin black dot-dash line
^12 - thin black dot-dot-dash line


            4. Reference Point File (.rep)

Example:
		51.533333	   0.166666	40000	40000	100	London
		55.900000	  -3.066666	40000	40000	70	Edinburg

Each line of the file contains information about one reference point:

    latitude  longitude  name_max_step  dot_max_step  point_size  point_name

"name_max_step" and "dot_max_step" - the maximum grid step in thousandths of a degree, above which 
the point is not drawn. Meaningful maximum step values range from 3 (approximately equal to the 
smallest grid step) and up to 40,000 (the largest step used)

"size_point" is indicated as a percentage of the maximum (from 0 to 100); The maximum size is set 
in the program interface.

Note: The grid spacing can be: 40°, 30°, 20°, 10°, 5°, 4°, 3°, 2°, 1° (integer degrees) 
and fractions of a degree: 0.75° (45'), 0.5° (30'), 0.25° (15'), 0.166(6)° (10'), 0.1° (6'),
0.0833(3)° (5'), 0.05° (3'), 0.0166(6)° (1'), 0.0125° (45"), 0.0083(3)° (30"), 0.0041(6)° (15"),
0.0027(7) (10"), 0.0016(6)° (6"), 0.0013(8)° (5"), 0.0008(3)° (3"), 0.0002(7)° (1")


            5. Target Marks File (.dat)

Example:
    All_Points
    43.63908 145.37371 ^0
    44.52445 146.58777 ^0
    44.513060  146.165000 ^1 0.1
    44.509440  146.157780 ^2 0.2
    44.509440  146.168330 ^3 0.3
    44.505830  146.166940 ^4 0.4
    44.504440  146.182780 ^11 0.5
    44.503330  146.157500 ^12 0.6
    44.503060  146.187220 ^13 0.7
    44.499720  146.156110 ^14 0.8
    44.497500  146.149440 ^1 0.9
    44.490560  146.196390 ^2 1
    44.488610  146.106670 ^3 1.1

The first line of the file contains the name for the entire file (unused now).
The following are the lines, each of which describes a separate mark:

     latitude longitude ^style size

The program reads the marks file, determines the necessary area of the map to display all marks,
determines the convenient grid step and generates the output .bmp file with the desired map fragment.

The "^Style" mark may be:
^0  - invisible mark (not shown, but used to determine the size of the map; can be used, for example,
      to set the minimum displayed part of the map)
^1  - black circle with a thin white contour (the contour is needed for visual separation when applying)
^2  - black square with a thin white outline
^3  - black diamond with a thin white outline
^4  - black triangle with a thin white contour
^11 - gray circle with a thin black contour
^12 - gray square with a thin black contour
^13 - a gray rhombus with a thin black contour
^14 - gray triangle with a thin black contour

Note: a minimum of 256 color bitmap is needed to display gray marks, a monochromatic image is not 
suitable for this. This, in turn, increases the size of the bitmap (about 500 MB for the bitmap is
the maximum, after which problems arise when working with it; for example, for 256 colors and 600 dpi,
the size of 1x0.9 meters is close to the limit)

The "size" of the mark is set as a multiplier from 0.1 to 10 to the "standard size" of the mark set 
in the program interface.
