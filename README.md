# Pseudo Systemd
Pseudo systemd is a mock service manager designed for testing purposes. 
It will run systemd commands on Docker. It simulates the behavior of the systemd service manager,
allowing you to start, stop, restart services as if you were working with the 
real systemd, this makes it an useful with Docker containers.

# How it works
This application fetches service files located in the `/etc/systemd/system` directory. It then parses 
these files and executes the command found in the `ExecStart` tag, within the specified `WorkingDirectory`.

# How I use it with Docker
I first compile a static amd64 version and then transfer it as `/usr/bin/systemctl` 
with executable permissions.

# TODO
- Implement `restart`
- Support multiple Environment Variables
- Trim key/values from .service files

# Build
gcc -static systemctl.c -o systemctl