#!/bin/sh
### BEGIN INIT INFO
# Provides:          pilight_console
# Required-Start:    $remote_fs dbus
# Required-Stop:     $remote_fs dbus
# Should-Start:	     $syslog
# Should-Stop:       $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: pilight-console
# Description:       LCD Display interface for pilight 
### END INIT INFO

PATH=/sbin:/bin:/usr/sbin:/usr/bin
DESC="pilight-console Daemon"
NAME="pilight-console"
DAEMON="/usr/bin/$NAME"
SCRIPTNAME=/etc/init.d/$NAME
CONFIGFILE=
PIDFILE="/var/run/pilight-console.pid"
DAEMON_ARGS=

# Gracefully exit if the package has been removed.
test -x $DAEMON || exit 0

. /lib/lsb/init-functions



#
#       Function that starts the daemon/service.
# Return
#   0 if daemon has been started
#   1 if daemon was already running
#   2 if daemon could not be started
#
d_start() {
	modprobe ch341 
#    ($DAEMON &) && return 0

	start-stop-daemon --start --quiet --pidfile $PIDFILE --exec $DAEMON --test > /dev/null \
    || return 1
	start-stop-daemon --start --quiet --pidfile $PIDFILE --exec $DAEMON $DAEMON_ARGS \
	        || return 2
	    # Add code here, if necessary, that waits for the process to be ready
	    # to handle requests from services started subsequently which depend
	    # on this one.  As a last resort, sleep for some time.
	}

#
#       Function that stops the daemon/service.
#
d_stop() {
#	kill `pidof $NAME`
    # Return
    #   0 if daemon has been stopped
    #   1 if daemon was already stopped
    #   2 if daemon could not be stopped
    #   other if a failure occurred
    start-stop-daemon --stop --quiet --retry=TERM/30/KILL/5 --pidfile $PIDFILE
    RETVAL="$?"
    # Many daemons don't delete their pidfiles when they exit.
    rm -f $PIDFILE
#    killall socat
    return "$RETVAL"
}


case "$1" in
    start)
        log_daemon_msg "Starting $DESC" "$NAME"
        d_start
        log_end_msg $?
        ;;
    stop)
        log_daemon_msg "Stopping $DESC" "$NAME"
        d_stop
        log_end_msg $?
        ;;
    *)
        echo "Usage: $SCRIPTNAME {start|stop}" >&2
        exit 1
        ;;
esac

exit 0
