from fabric.api import run, local
from fabric.context_managers import hide


def start_ufod(cenv=None, limit=None):
    exports = ""
    n_gpus = 0

    with hide('stdout'):
        smi_output = run('nvidia-smi -L')
        n_gpus = len(smi_output.split('\n'))

    n_gpus = min(n_gpus, int(limit)) if limit else n_gpus

    if cenv:
        cenv_path = "$HOME/.local/share/cenv/{}".format(cenv)
        exports = "PATH=$PATH:{cenv}/bin LD_LIBRARY_PATH={cenv}/lib".format(cenv=cenv_path)

    addresses = ['tcp://141.52.65.18:{}'.format(6666 + i) for i in range(n_gpus)]

    for i in range(n_gpus):
        address = 'tcp://141.52.65.18:{}'.format(6666 + i)
        run("{} bash -c '((nohup ufod -l {} > /dev/null 2> /dev/null) &)'".format(exports, address), pty=False)

    return addresses


def start(cmd='ufo-launch', args=None, cenv=None, limit=None):
    addresses = start_ufod(cenv=cenv, limit=limit)
    cmd_line = '{} {}'.format(cmd, ' '.join('-a {}'.format(a) for a in addresses))

    if args:
        cmd_line = '{} {}'.format(cmd_line, args)

    local(cmd_line)
