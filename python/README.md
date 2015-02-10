## About

The Python tools for UFO, are addons to make interaction with
[UFO](https://github.com/ufo-kit/ufo-core) easier.


### Numpy integration

UFO and Numpy buffers can be converted freely between each other:

```python
import numpy as np
import ufo.numpy

# Make Ufo.Buffer from Numpy buffer
a = np.ones((640, 480))
b = ufo.numpy.fromarray(a)

# Make Numpy buffer from Ufo.Buffer
a = ufo.numpy.asarray(b)
```


### Simpler task setup

Creating tasks becomes as simple as importing a class:

```python
from ufo import Reader, Backproject, Writer

read = Reader(path='../*.tif')
backproject = Backproject(axis_pos=512.2)
write = Writer(filename='foo-%05i.tif')
```

Hooking up tasks is wrapped by calls. You can use the outer call to schedule
execution:

```python
scheduler = Ufo.Scheduler()
scheduler.run(write(backproject(read())).graph)
```

or use the `run` method to launch execution asynchronously. `join` on the result
if you want to wait:

```python
write(backproject(read())).run().join()
```

If no final endpoint is specified, you must iterate over the data:

```python
for item in backproject(read()):
    print np.mean(item)
```

Similarly you can also input data by specyifing an iterable of NumPy arrays

```python
sinogram = np.ones((512, 512))

for slice_data in backproject([sinogram]):
    print slice_data
```
