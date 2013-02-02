.. _using-examples:

========
Examples
========

Global shift detection using image correlation
==============================================

A shift in vertical and horizontal direction of two images in a sequence can be
estimated as the largest argument of a cross-correlation between those images. We
can improve performance by employing a transform into the Fourier domain first
and multiply the frequencies as in 

.. math::

    \textrm{argmax}\ \mathcal{F}^{-1}\{\mathcal{F}\{I_1\} \cdot (\mathcal{F}\{I_2\})^\star \}.
    
To compute the image correlation between all pairs in the image sequence, we
have to set the ``copy`` parameter of the ``demux`` filter to ``delayed``, which
means, that for input sequence :math:`S = (I_1, \ldots, I_n)`, output 1
transmits sequence :math:`S_1 = (I_1, \ldots, I_{n-1})` and output 2 transmits
sequence :math:`S_2 = (I_2, \ldots, I_{n})`. The code could look like this::

    from gi.repository import Ufo
    
    g = Ufo.Graph()
    
    # instantiate the filters and add them as global names
    m = globals()
    filters = ['reader', 'writer', 'normalize', 'demux', 'fft', 'ifft', 'argmax']
    for f in filters:
        m[f] = g.get_filter(f)

    # here we want to choose the names on our own
    cmul = g.get_filter('complex')
    conj = g.get_filter('complex')

    # choose path to read and write
    reader.set_properties(path='/home/user/data/')
    writer.set_properties(path='/home/user/data/processed')
    
    demux.set_properties(copy='delayed')
    fft.set_properties(dimensions=2)
    ifft.set_properties(dimensions=2)
    cmul.set_properties(operation='mul')
    conj.set_properties(operation='conj')

    reader.connect_to(normalize)
    normalize.connect_to(fft)
    fft.connect_to(demux)
    
    # First argument of complex multiplication is kept intact
    demux.connect_by_name('output1', cmul, 'input1')
    # Second argument must be conjugated ...
    demux.connect_by_name('output2', conj, 'default')
    # ... and then multiplied
    conj.connect_by_name('default', cmul, 'input2')
    cmul.connect_to(ifft)
    
    # Return to spatial representation and look for maximum argument
    ifft.connect_to(argmax)
    argmax.connect_to(writer)

    g.run()
