# Resourcer

Resourcer.exe lists and exports resources in a portable executable (PE) file.
```
Usage:

    Resourcer.exe srcFile -list [-t resType] [-c|-ct]
    Resourcer.exe srcFile -export -t resType {-n resName | -i resID} -o outfile

srcFile must be the first parameter: the path to a portable executable (PE) file with resources.
-list or -export must be the second parameter.

With -list:
  resType : if *, lists all resources;
            if a particular resource type, lists resources of that type
                (see list below for predefined types);
            if omitted, lists resources with named (non-MAKEINTRESOURCE) types.
  -c      : Output as comma-delimited text.
  -ct     : Output as tab-delimited text.

With -export:
  resType : resource type (see list below for predefined types).
  resName : resource name.
  resID   : resource name as a MAKEINTRESOURCE integer value.
  outFile : file path to export the resource.

Known pre-defined MAKEINTRESOURCE types are:
    RT_CURSOR
    RT_BITMAP
    RT_ICON
    RT_MENU
    RT_DIALOG
    RT_STRING
    RT_FONTDIR
    RT_FONT
    RT_ACCELERATOR
    RT_RCDATA
    RT_MESSAGETABLE
    RT_GROUP_CURSOR
    RT_GROUP_ICON
    RT_VERSION
    RT_DLGINCLUDE
    RT_PLUGPLAY
    RT_VXD
    RT_ANICURSOR
    RT_ANIICON
    RT_HTML
    RT_MANIFEST
```