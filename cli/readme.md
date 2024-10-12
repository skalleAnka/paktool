% PAKTOOL(1)
% skalleAnka
% October 2024

# NAME
paktool - Create, extract, convert and compare Quake/Quake 2/Quake 3 pack files.

# SYNOPSIS
**paktool** [**-h** | **-x** | **-c** | **-l** | **-\-compare**] [**-i** *input_file*]... [**-o** *output_file*]

# DESCRIPTION
**paktool** is a tool that can be used to create, extract, compare, convert and list contents of pack files. It supports *.pak* from *Quake* and *Quake 2* as well as *pk3* from *Quake 3*. It does *not* support *.pak* files from *S!N* or *Daikatana*. There is also support for *.grp* packs from Build engine games.

The type of pack is inferred from file extensions given with **-i** and **-o**, and inputs or outputs without extensions are interpreted to be folders.

# OPTIONS
Action is selected by specifying one of **-h**, **-x**, **-c**, **-l** or **-\-compare**. One or more input files/directories are specified with **-i** and an output file/directory is specified with **-o**.

**-h**, **-\-help**
:	Display a short description of the options that can be used.

**-x**, **-\-extract**
:	Extract the content of the packs specified with **-i**. If **-o** is used, it should specify an existing directory. If **-o** is not used, the current working directory will be used as output. A new sub folder named after the pack will be created for each input pack, and the extracted contents will be placed there.

**-c**, **-\-convert**
:	Convert one pack format to another or create a new pack. Specify packs to convert with **-i** and the output pack with **-o**. If the output already exists, an error will be displayed. A new pack is created by having the inputs be one or more folders.

:	Output format is determined by file extension. Not specifying an extension becomes an extraction of the inputs to a new folder.

:	When multiple inputs are specified, it works like the Quake engines in the way that files in earlier inputs are ignored if they also exist in later inputs, and only the latest one will be converted to the new pack.

:	Pak files have, by tradition, a file count limit of 2048 files. If converting to *.pak* and this limit is reached, more output *.pak* files will be created with increasing numbers if the output pack has the **pakN.pak** format, otherwise an error will be displayed.

:   It's probably not that useful to try to convert from other formats to Build engine GRP files as those are restricted to DOS 8.3 file names without directories.

**-l**, **-\-list**
:	List contents of the packs specified with **-i**

**-\-compare**
:	Compare two packs specified with **-i**. This detects if a file is different in two packs, if the file exists under one or more different names in the other pack, or if it is missing altogether from one of them. The input packs don't need to be the same type and can be a folder.

# EXAMPLES
**$ paktool -l -i pak0.pak**
:	Lists the contents of *pak0.pak* in the current directory to stdout.

**$ paktool -x -i pak0.pak**
:	Extract *pak0.pak* in the current directory to a new folder *pak0* in the current directory.

**$ paktool -x -i /usr/share/quake/pak0.pak -o /home/bob** 
:	Extract *pak0.pak* in */usr/share/quake* to a new folder *pak0* in */home/bob*.

**$ paktool -c -i /usr/share/quake/pak0.pak -i /usr/share/quake/pak1.pak -o /home/bob/pak0.pk3** 
:	Convert *pak0.pak* and *pak1.pak* in */usr/share/quake* to a new merged file *pak0.pk3* in */home/bob*.

**$ paktool -c -i /usr/share/quake/pak0.pak -i /usr/share/quake/pak1.pak -o /home/bob/mypack** 
:	Extract *pak0.pak* and *pak1.pak* in */usr/share/quake* to a new merged folder *mypack* in */home/bob*.

**$ paktool -c -i pak0.pk3 -o /home/bob/pak0.pak** 
:	Convert *pak0.pk3* in the current directory to a new file *pak0.pak* in */home/bob*.

**$ paktool -\-compare -i /usr/share/quake/pak0.pak -i /home/bob/pak0.pk3** 
:	Compare *pak0.pak* in */usr/share/quake* to *pak0.pk3* in */home/bob*.
 

# NOTES
When .pk3 files are created, the contents is compressed with the highest zip compression. This is true for all files except **jpg**, **jpeg**, **png**, **mp3**, **ogg**, **opus** and **flac** files. These file types are commonly used by modern Quake ports and are already compressed. They will be recognized by extension and stored without further compression inside the .pk3 file.