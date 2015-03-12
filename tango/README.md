## Installation

Install the server script with

    $ python setup.py install

and create a new TANGO server `Ufo/xyz` with a class named `Process`.


## Usage

Start the device server with

    $ Ufo foo

Now you can specify the graph with the JSON description string and execute it
using the `Run` command. `Run` returns the job ID which you can use to check if
the process is ongoing. Here's a simple example in Python:

```python
import PyTango
import sleep

json = "{}"     # add real description

process = PyTango.DeviceProxy("foo/process/1")
pid = process.Run()

while process.Running(pid):
    time.sleep(0.5)
```
