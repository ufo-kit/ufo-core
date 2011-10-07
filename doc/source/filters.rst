.. _filters:

=======
Filters
=======

UFO filters are simple shared objects that expose their ``GType`` and implement
the :ref:`UfoFilter <ufo-api>` class. When the UFO framework is initialized, plugin
description files with the suffix `ufo-plugin` are inspected. These description
files are simple INI files looking like this::

    [UFO Plugin]
    Name=fft
    Module=filterfft
    IAge=1
    Authors=John Doe
    Copyright=Public Domain
    Website=http://ufo.kit.edu
    Description=Fast Fourier Transformation

The `Module` entry specifies the name of the shared object, in this case a
`libfilterfft.so` is looked up in all suitable paths.


reader
======

`Purpose`
    Reads TIFF files from disk and converts them to the internal 32-bit floating
    point format.

`Input`
    None

`Output`
    UfoBuffer with file content

`Properties`
    "path" [`type` : string, `default` : "."]
        Path to files to load from

    "prefix" [`type` : string, `default` : ""]
        Reader loads only those files whose prefix matches the specified prefix

    "count" [`type` : integer, `default` : 1]
        Number of files to load from `path`


writer
======

`Purpose`
    Writes TIFF files.

`Input`
    UfoBuffer to write 

`Output`
    None

`Properties`
    "path" [`type` : string, `default` : "."]
        Path to files to load from

    "prefix" [`type` : string, `default` : ""]
        Prefix the output filenames with this prefix. The filename also contains
        the current counter.


TODO
====

We should pull out all this information from the source using `gtk-doc`.
