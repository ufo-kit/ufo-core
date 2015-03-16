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

# alternatively you could wait for completion
process.Wait(pid)

print("Exit code: {}".format(process.ExitCode(pid)))
```


## Continuous computation

Instead of running a single job once, you can keep it "alive" by calling
`RunContinuous` instead of `Run`. Unlike `Run`, `RunContinuous` will not start
computation immediately but must be triggered with the `Continue` command. Each
call to `Continue` transmits the *current* JSON attribute, which means you can
update that in between calls to `Continue`. Note, that you *must* call `Stop` in
order to release the resources allocated by the compute process. Here is an
example:

```python
pid = process.RunContinuous()
process.Continue(pid)
process.Wait(pid)

process.json = "..."    # Updated description
process.Continue(pid)
process.Wait(pid)

process.Stop(pid)
```

The only possible update operation on the JSON description is changes of
parameters. Sending a completely new description will result in undefined
behaviour.
