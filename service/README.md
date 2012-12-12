# Slackware service commands
## About
The objective of this project is to emulate two command present in another Linux Distros: service and insserv.
The service command allows one to start, stop, restart, ... services without writing the path ("/etc/rc.d/").
The insserv command prepares the system to start and stop a service at boot and shutdown respectevely.

## Service command
The command starts by creating a list of parameters to pass to the service.
After that it looks for the service in "/etc/rc.d/".
It searchs using the "service name" and "rc.service name", e.g. searchs for openfire and rc.openfire.
Finally check if the service is executable and if so invoke the service with the parameters list.

## Insserv command
The command starts by checking if the service is in the correct folder ("/etc/rc.d/").
If not it creates a symbolic link in "/etc/rc.d" and pointing to the service.
After that it inserts a start and stop command in "/etc/rc.d/rc.local" and "/etc/rc.d/rc.local_shutdown".
