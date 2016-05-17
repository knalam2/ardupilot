#!/usr/bin/env python
"""
 Framework to start a simulated vehicle and connect it to MAVProxy
 Peter Barker, April 2016
 based on sim_vehicle.sh by Andrew Tridgell, October 2011
"""

import atexit
import getpass
import optparse
import os
import signal
import subprocess
import sys
import tempfile
import time


class CompatError(Exception):
    """A custom exception class to hold state if we encounter the parse error we are looking for"""
    def __init__(self, error, opts, rargs):
        Exception.__init__(self, error)
        self.opts = opts
        self.rargs = rargs


class CompatOptionParser(optparse.OptionParser):
    """An option parser which emulates the behaviour of the old sim_vehicle.sh; if passed -C, the first argument not understood starts a list of arguments that are passed straight to mavproxy"""

    def __init__(self, *args, **kwargs):
        optparse.OptionParser.__init__(self, *args, **kwargs)

    def error(self, error):
        """Override default error handler called by optparse.OptionParser.parse_args when a parse error occurs; raise a detailed exception which can be caught"""
        if error.find("no such option") != -1:
            raise CompatError(error, self.values, self.rargs)

        optparse.OptionParser.error(self, error)

    def parse_args(self, args=None, values=None):
        """Wrap parse_args so we can catch the exception raised upon discovering the known parameter parsing error"""
        try:
            opts, args = optparse.OptionParser.parse_args(self)
        except CompatError as e:
            if not e.opts.sim_vehicle_sh_compatible:
                print(e)
                print("Perhaps you want --sim_vehicle_sh_compatible (-C)?")
                sys.exit(1)
            if e.opts.mavproxy_args:
                print("--mavproxy-args not permitted in compat mode")
                sys.exit(1)

            args = []
            opts = e.opts
            mavproxy_args = [str(e)[16:]]  # this trims "no such option" off
            mavproxy_args.extend(e.rargs)
            opts.ensure_value("mavproxy_args", " ".join(mavproxy_args))

        return opts, args


def cygwin_pidof(proc_name):
    """ Thanks to kata198 for this:
    https://github.com/kata198/cygwin-ps-misc/blob/master/pidof
    """
    pipe = subprocess.Popen("ps -ea | grep " + proc_name, shell=True, stdout=subprocess.PIPE)
    output_lines = pipe.stdout.read().replace("\r", "").split("\n")
    ret = pipe.wait()
    pids = []
    if ret != 0:
        # No results
        return []
    for line in output_lines:
        if not line:
            continue
        line_split = [item for item in line.split(' ') if item]
        cmd = line_split[-1].split('/')[-1]
        if cmd == proc_name:
            try:
                pid = int(line_split[0].strip())
            except:
                pid = int(line_split[1].strip())
            str_pid = str(pid)
            if str_pid not in pids:
                pids.append(str_pid)
    return pids


def under_cygwin():
    """Return Cygwin path"""
    return os.path.exists("/usr/bin/cygstart")


def kill_tasks_cygwin(victims):
    """Clean cygwin tasks"""
    for victim in list(victims):
        pids = cygwin_pidof(victim)
#        progress("pids for (%s): %s" % (victim,",".join([ str(p) for p in pids])))
        for apid in pids:
            os.kill(apid, signal.SIGKILL)


def kill_tasks():
    """Clean up stray processes by name.  This is a somewhat shotgun approach"""
    victim_names = {'JSBSim', 'lt-JSBSim', 'ArduPlane.elf', 'ArduCopter.elf', 'APMrover2.elf', 'AntennaTracker.elf',
                    'JSBSIm.exe', 'MAVProxy.exe', 'runsim.py', 'AntennaTracker.elf'}

    if under_cygwin():
        return kill_tasks_cygwin(list(victim_names))

    import psutil
    for proc in psutil.process_iter():
        if proc.status() == psutil.STATUS_ZOMBIE:
            continue
        if proc.name() in victim_names:
            proc.kill()

# clean up processes at exit:
atexit.register(kill_tasks)


def check_jsbsim_version():
    """Assert that the JSBSim we will run is the one we expect to run"""
    jsbsim_cmd = ["JSBSim", "--version"]
    progress_cmd("Get JSBSim version", jsbsim_cmd)
    try:
        jsbsim_version = subprocess.Popen(jsbsim_cmd, stdout=subprocess.PIPE).communicate()[0]
    except OSError as e:
        jsbsim_version = ''  # this value will trigger the ".index"
        # check below and produce a reasonable
        # error message
    try:
        jsbsim_version.index("ArduPilot")
    except ValueError:
        print(r"""
=========================================================
You need the latest ArduPilot version of JSBSim installed
and in your \$PATH

Please get it from git://github.com/tridge/jsbsim.git
See
 http://dev.ardupilot.org/wiki/simulation-2/sitl-simulator-software-in-the-loop/setting-up-sitl-on-linux/
for more details
=========================================================
""")
        sys.exit(1)


def progress(text):
    """Display sim_vehicle progress text"""
    print("SIM_VEHICLE: " + text)


def find_autotest_dir():
    """Return path to autotest directory"""
    return os.path.dirname(os.path.realpath(__file__))


def find_root_dir():
    """return path to root directory"""
    return os.path.realpath(os.path.join(find_autotest_dir(), '../..'))

progress("Start")

# define and run parser
parser = CompatOptionParser("sim_vehicle.py", epilog="""
    eeprom.bin in the starting directory contains the parameters for your simulated vehicle. Always start from the same directory. It is recommended that you start in the main vehicle directory for the vehicle you are simulating, for example, start in the ArduPlane directory to simulate ArduPlane
""")

parser.add_option("-v", "--vehicle", type='string', default=None, help="vehicle type (ArduPlane, ArduCopter or APMrover2)")
parser.add_option("-f", "--frame", type='string', default=None, help="set aircraft frame type for copters can choose +, X, quad or octa for planes can choose elevon or vtail")
parser.add_option("-C", "--sim_vehicle_sh_compatible", action='store_true', default=False, help="be compatible with the way sim_vehicle.sh works; make this the first option")
parser.add_option("-H", "--hil", action='store_true', default=False, help="start HIL")

group_build = optparse.OptionGroup(parser, "Build options")
group_build.add_option("-N", "--no-rebuild", action='store_true', default=False, help="don't rebuild before starting ardupilot")
group_build.add_option("-D", "--debug", action='store_true', default=False, help="build with debugging")
group_build.add_option("-c", "--clean", action='store_true', default=False, help="do a make clean before building")
group_build.add_option("-j", "--jobs", default=None, type='int', help='number of processors to use during build (default for waf : number of processor, for make : 1)')
group_build.add_option("-b", "--build-target", default=None, type='string', help="override SITL build target")
group_build.add_option("-s", "--build-system", default="waf", type='choice', choices=["make", "waf"], help="build system to use")
group_build.add_option("", "--no-rebuild-on-failure", dest="rebuild_on_failure", action='store_false', default=True, help="if build fails, do not clean and rebuild")
parser.add_option_group(group_build)

group_sim = optparse.OptionGroup(parser, "Simulation options")
group_sim.add_option("-I", "--instance", default=0, type='int', help="instance of simulator")
group_sim.add_option("-V", "--valgrind", action='store_true', default=False, help="enable valgrind for memory access checking (very slow!)")
group_sim.add_option("-T", "--tracker", action='store_true', default=False, help="start an antenna tracker instance")
group_sim.add_option("-A", "--sitl-instance-args", type='string', default=None, help="pass arguments to SITL instance")
# group_sim.add_option("-R", "--reverse-throttle", action='store_true', default=False, help="reverse throttle in plane")
group_sim.add_option("-G", "--gdb", action='store_true', default=False, help="use gdb for debugging ardupilot")
group_sim.add_option("-g", "--gdb-stopped", action='store_true', default=False, help="use gdb for debugging ardupilot (no auto-start)")
group_sim.add_option("-d", "--delay-start", default=0, type='float', help="delays the start of mavproxy by the number of seconds")
group_sim.add_option("-B", "--breakpoint", type='string', action="append", default=[], help="add a breakpoint at given location in debugger")
group_sim.add_option("-M", "--mavlink-gimbal", action='store_true', default=False, help="enable MAVLink gimbal")
group_sim.add_option("-L", "--location", type='string', default='CMAC', help="select start location from Tools/autotest/locations.txt")
group_sim.add_option("-l", "--custom-location", type='string', default=None, help="set custom start location")
group_sim.add_option("-S", "--speedup", default=1, type='int', help="set simulation speedup (1 for wall clock time)")
group_sim.add_option("-t", "--tracker-location", default='CMAC_PILOTSBOX', type='string', help="set antenna tracker start location")
group_sim.add_option("-w", "--wipe-eeprom", action='store_true', default=False, help="wipe EEPROM and reload parameters")
group_sim.add_option("-m", "--mavproxy-args", default=None, type='string', help="additional arguments to pass to mavproxy.py")
group_sim.add_option("", "--strace", action='store_true', default=False, help="strace the ArduPilot binary")
parser.add_option_group(group_sim)


# special-cased parameters for mavproxy, because some people's fingers
# have long memories, and they don't want to use -C :-)
group = optparse.OptionGroup(parser, "Compatibility MAVProxy options (consider using --mavproxy-args instead)")
group.add_option("", "--out", default=[], type='string', action="append", help="create an additional mavlink output")
group.add_option("", "--map", default=False, action='store_true', help="load map module on startup")
group.add_option("", "--console", default=False, action='store_true', help="load console module on startup")
parser.add_option_group(group)

cmd_opts, cmd_args = parser.parse_args()

if cmd_opts.sim_vehicle_sh_compatible and cmd_opts.jobs is None:
    cmd_opts.jobs = 1

# validate parameters
if cmd_opts.hil:
    if cmd_opts.valgrind:
        print("May not use valgrind with hil")
        sys.exit(1)
    if cmd_opts.gdb or cmd_opts.gdb_stopped:
        print("May not use gdb with hil")
        sys.exit(1)
    if cmd_opts.strace:
        print("May not use strace with hil")
        sys.exit(1)

if cmd_opts.valgrind and (cmd_opts.gdb or cmd_opts.gdb_stopped):
    print("May not use valgrind with gdb")
    sys.exit(1)

if cmd_opts.strace and (cmd_opts.gdb or cmd_opts.gdb_stopped):
    print("May not use strace with gdb")
    sys.exit(1)

if cmd_opts.strace and cmd_opts.valgrind:
    print("valgrind and strace almost certainly not a good idea")

# magically determine vehicle type (if required):
if cmd_opts.vehicle is None:
    cwd = os.getcwd()
    cmd_opts.vehicle = os.path.basename(cwd)

# determine a frame type if not specified:
default_frame_for_vehicle = {
    "APMrover2": "rover",
    "ArduPlane": "jsbsim",
    "ArduCopter": "quad",
    "AntennaTracker": "tracker"
}

# try to validate vehicle
if cmd_opts.vehicle not in default_frame_for_vehicle:
    progress("** Is (%s) really your vehicle type?  Try  -v VEHICLETYPE  if not, or be in the e.g. ArduCopter subdirectory" % (cmd_opts.vehicle,))

# determine frame options (e.g. build type might be "sitl")
if cmd_opts.frame is None:
    cmd_opts.frame = default_frame_for_vehicle[cmd_opts.vehicle]

# setup ports for this instance
mavlink_port = "tcp:127.0.0.1:" + str(5760 + 10 * cmd_opts.instance)
simout_port = "127.0.0.1:" + str(5501 + 10 * cmd_opts.instance)

"""
make_target: option passed to make to create binaries.  Usually sitl, and "-debug" may be appended if -D is passed to sim_vehicle.py
default_params_filename: filename of default parameters file.  Taken to be relative to autotest dir.
extra_mavlink_cmds: extra parameters that will be passed to mavproxy
"""
_options_for_frame = {
    "+": {
        "waf_target": "bin/arducopter-quad",
        "default_params_filename": "copter_params.parm"
    },
    "quad": {
        "model": "+",
        "waf_target": "bin/arducopter-quad",
        "default_params_filename": "copter_params.parm"
    },
    "X": {
        "waf_target": "bin/arducopter-quad",
        # this param set FRAME doesn't actually work because mavproxy
        # won't set a parameter unless it knows of it, and the param fetch happens asynchronously
        "extra_mavlink_cmds": "param fetch frame; param set FRAME 1;",
        "default_params_filename": "copter_params.parm"
    },
    "heli-dual": {
        "make_target": "sitl-heli-dual",
        "waf_target": "bin/arducopter-coax",  # is this correct? -pb201604301447
    },
    "heli-compound": {
        "make_target": "sitl-heli-compound",
        "waf_target": "bin/arducopter-coax",  # is this correct? -pb201604301447
    },
    "IrisRos": {
        "default_params_filename": "copter_params.parm",
        "waf_target": "bin/arducopter-quad",
    },
    "Gazebo": {
        "default_params_filename": "copter_params.parm",
        "waf_target": "bin/arducopter-quad",
    },

    "octa": {
        "make_target": "sitl-octa",
        "waf_target": "bin/arducopter-octa",
        "default_params_filename": "copter_params.parm",
    },
    "tri": {
        "make_target": "sitl-tri",
        "waf_target": "bin/arducopter-tri",
        "default_params_filename": "tri_params.parm",
    },
    "y6": {
        "make_target": "sitl-y6",
        "waf_target": "bin/arducopter-y6",
        "default_params_filename": "y6_params.parm",
    },
    "firefly": {
        "default_params_filename": "firefly.parm",
        "waf_target": "bin/arducopter-firefly",
    },
    "heli": {
        "make_target": "sitl-heli",
        "waf_target": "bin/arducopter-heli",
        "default_params_filename": "Helicopter.parm",
    },
    "last_letter": {
        "waf_target": "bin/arduplane",
    },
    "CRRCSim": {
        "waf_target": "bin/arduplane",
    },
    "jsbsim": {
        "waf_target": "bin/arduplane",
        "default_params_filename": "ArduPlane.parm",
    },
    "quadplane-tilttri": {
        "build_target": "sitl-tri",
        "default_params_filename": "quadplane-tilttri.parm",
    },
    "quadplane": {
        "waf_target": "bin/arduplane",
        "default_params_filename": "quadplane.parm",
    },
    "plane-elevon": {
        "waf_target": "bin/arduplane",
        "default_params_filename": "plane-elevons.parm",
    },
    "plane-vtail": {
        "waf_target": "bin/arduplane",
        "default_params_filename": "plane-vtail.parm",
    },
    "plane": {
        "waf_target": "bin/arduplane",
        "default_params_filename": "plane.parm",
    },
    "rover": {
        "waf_target": "bin/ardurover",
        "default_params_filename": "Rover.parm",
    },
}

_default_waf_target = {
    "ArduPlane": "bin/arduplane",
    "ArduCopter": "bin/arducopter-quad",
    "APMrover2": "bin/ardurover",
    "AntennaTracker": "bin/antennatracker",
}


def default_waf_target(vehicle):
    """Returns a waf target based on vehicle type, which is often determined by which directory the user is in"""
    return _default_waf_target[vehicle]


def options_for_frame(frame, vehicle, opts):
    """Return informatiom about how to sitl for frame e.g. build-type==sitl"""
    ret = None
    if frame in _options_for_frame:
        ret = _options_for_frame[frame]
    else:
        for p in ["octa", "tri", "y6", "firefly", "heli", "last_letter", "jsbsim", "quadplane", "plane-elevon", "plane-vtail", "plane"]:
            if frame.startswith(p):
                ret = _options_for_frame[p]
                break
    if ret is None:
        if frame.endswith("-heli"):
            ret = _options_for_frame["heli"]
    if ret is None:
        ret = {}

    if "model" not in ret:
        ret["model"] = frame

    if "make_target" not in ret:
        ret["make_target"] = "sitl"

    if "waf_target" not in ret:
        ret["waf_target"] = default_waf_target(vehicle)

    if opts.build_target is not None:
        ret["make_target"] = opts.build_target
        ret["waf_target"] = opts.build_target

    return ret


def do_build_waf(opts, frame_options):
    """build sitl using waf"""
    progress("WAF build")

    old_dir = os.getcwd()
    root_dir = find_root_dir()
    os.chdir(root_dir)

    waf_light = os.path.join(root_dir, "modules/waf/waf-light")

    cmd_configure = [waf_light, "configure", "--board", "sitl"]
    if opts.debug:
        cmd_configure.append("--debug")

    run_cmd_blocking("Configure waf", cmd_configure)

    if opts.clean:
        run_cmd_blocking("Building clean", [waf_light, "clean"])

    cmd_build = [waf_light, "build", "--target", frame_options["waf_target"]]
    if opts.jobs is not None:
        cmd_build += ['-j', str(opts.jobs)]

    _, sts = run_cmd_blocking("Building", cmd_build)

    if sts != 0:  # build failed
        if opts.rebuild_on_failure:
            progress("Build failed; cleaning and rebuilding")
            run_cmd_blocking("Building clean", [waf_light, "clean"])

            _, sts = run_cmd_blocking("Building", cmd_build)
            if sts != 0:
                progress("Build failed")
                sys.exit(1)
        else:
            progress("Build failed")
            sys.exit(1)

    os.chdir(old_dir)


def do_build(vehicledir, opts, frame_options):
    """Build build target (e.g. sitl) in directory vehicledir"""

    if opts.build_system == 'waf':
        return do_build_waf(opts, frame_options)

    old_dir = os.getcwd()

    os.chdir(vehicledir)

    if opts.clean:
        run_cmd_blocking("Building clean", ["make", "clean"])

    build_target = frame_options["make_target"]
    if opts.debug:
        build_target += "-debug"

    build_cmd = ["make", build_target]
    if opts.jobs is not None:
        build_cmd += ['-j', str(opts.jobs)]

    _, sts = run_cmd_blocking("Building %s" % (build_target), build_cmd)
    if sts != 0:
        progress("Build failed; cleaning and rebuilding")
        run_cmd_blocking("Cleaning", ["make", "clean"])
        _, sts = run_cmd_blocking("Building %s" % (build_target), build_cmd)
        if sts != 0:
            progress("Build failed")
            sys.exit(1)

    os.chdir(old_dir)


def find_location_by_name(autotest, locname):
    """Search locations.txt for locname, return GPS coords"""
    locations_filepath = os.path.join(autotest, "locations.txt")
    for line in open(locations_filepath, 'r'):
        line = line.rstrip("\n")
        (name, loc) = line.split("=")
        if name == locname:
            return loc
    print("Failed to find location (%s)" % (cmd_opts.location))
    sys.exit(1)


def progress_cmd(what, cmd):
    """Print cmd in a way a user could cut-and-paste to get the same effect"""
    progress(what)
    shell_text = "%s" % (" ".join(['"%s"' % x for x in cmd]))
    progress(shell_text)


def run_cmd_blocking(what, cmd):
    """Execute cmd and block until its end"""
    progress_cmd(what, cmd)
    p = subprocess.Popen(cmd)
    return os.waitpid(p.pid, 0)


def run_in_terminal_window(autotest, name, cmd):
    """Execute the run_in_terminal_window.sh command for cmd"""
    runme = [os.path.join(autotest, "run_in_terminal_window.sh"), name]
    runme.extend(cmd)
    progress_cmd("Run " + name, runme)
    p = subprocess.Popen(runme)  # bg this explicitly?!

tracker_uarta = None  # blemish


def start_antenna_tracker(autotest, opts):
    """Compile and run the AntennaTracker, add tracker to mavproxy"""
    global tracker_uarta
    progress("Preparing antenna tracker")
    tracker_home = find_location_by_name(find_autotest_dir(), opts.tracker_location)
    vehicledir = os.path.join(autotest, "../../" + "AntennaTracker")
    do_build(vehicledir, opts, "sitl-debug")
    tracker_instance = 1
    os.chdir(vehicledir)
    tracker_uarta = "tcp:127.0.0.1:" + str(5760+10*tracker_instance)
    exe = os.path.join(vehicledir, "AntennaTracker.elf")
    run_in_terminal_window(autotest, "AntennaTracker", ["nice", exe, "-I" + str(tracker_instance), "--model=tracker", "--home="+tracker_home])


def start_vehicle(binary, autotest, opts, stuff, loc):
    """Run the ArduPilot binary"""

    cmd_name = opts.vehicle
    cmd = []
    if opts.valgrind:
        cmd_name += " (valgrind)"
        cmd.append("valgrind")
    if opts.gdb:
        cmd_name += " (gdb)"
        cmd.append("gdb")
        gdb_commands_file = tempfile.NamedTemporaryFile(delete=False)
        atexit.register(os.unlink, gdb_commands_file.name)

        for breakpoint in opts.breakpoint:
            gdb_commands_file.write("b %s\n" % (breakpoint,))
        gdb_commands_file.write("r\n")
        gdb_commands_file.close()
        cmd.extend(["-x", gdb_commands_file.name])
        cmd.append("--args")
    if opts.strace:
        cmd_name += " (strace)"
        cmd.append("strace")
        strace_options = ['-o', binary + '.strace', '-s', '8000', '-ttt']
        cmd.extend(strace_options)

    cmd.append(binary)
    cmd.append("-S")
    cmd.append("-I"+str(opts.instance))
    cmd.extend(["--home", loc])
    if opts.wipe_eeprom:
        cmd.append("-w")
    cmd.extend(["--model", stuff["model"]])
    cmd.extend(["--speedup", str(opts.speedup)])
    if opts.sitl_instance_args:
        cmd.extend(opts.sitl_instance_args.split(" "))  # this could be a lot better..
    if opts.mavlink_gimbal:
        cmd.append("--gimbal")
    if "default_params_filename" in stuff:
        path = os.path.join(autotest, stuff["default_params_filename"])
        progress("Using defaults from (%s)" % (path,))
        cmd.extend(["--defaults", path])

    run_in_terminal_window(autotest, cmd_name, cmd)


def start_mavproxy(opts, stuff):
    """Run mavproxy"""
    # FIXME: would be nice to e.g. "mavproxy.mavproxy(....).run" rather than shelling out

    extra_cmd = ""
    cmd = []
    if under_cygwin():
        cmd.append("/usr/bin/cygstart")
        cmd.append("-w")
        cmd.append("/cygdrive/c/Program Files (x86)/MAVProxy/mavproxy.exe")
    else:
        cmd.append("mavproxy.py")

    if opts.hil:
        cmd.extend(["--load-module", "HIL"])
    else:
        cmd.extend(["--master", mavlink_port, "--sitl", simout_port])
    # If running inside of a vagrant guest, then we probably want to forward our mavlink out to the containing host OS
    if getpass.getuser() == "vagrant":
        cmd.extend(["--out", "10.0.2.2:14550"])
    for port in [14550, 14551]:
        cmd.extend(["--out", "127.0.0.1:"+str(port)])

    if opts.tracker:
        cmd.extend(["--load-module", "tracker"])
        global tracker_uarta
        # tracker_uarta is set when we start the tracker...
        extra_cmd += "module load map; tracker set port %s; tracker start; tracker arm;" % (tracker_uarta,)

    if opts.mavlink_gimbal:
        cmd.extend(["--load-module", "gimbal"])

    if "extra_mavlink_cmds" in stuff:
        extra_cmd += " " + stuff["extra_mavlink_cmds"]

    if opts.mavproxy_args:
        cmd.extend(opts.mavproxy_args.split(" "))  # this could be a lot better..

    # compatibility pass-through parameters (for those that don't want
    # to use -C :-)
    for out in opts.out:
        cmd.extend(['--out', out])
    if opts.map:
        cmd.append('--map')
    if opts.console:
        cmd.append('--console')

    if len(extra_cmd):
        cmd.extend(['--cmd', extra_cmd])

    run_cmd_blocking("Run MavProxy", cmd)
    progress("MAVProxy exitted")

frame_infos = options_for_frame(cmd_opts.frame, cmd_opts.vehicle, cmd_opts)

if frame_infos["model"] == "jsbsim":
    check_jsbsim_version()

vehicle_dir = os.path.realpath(os.path.join(find_root_dir(), cmd_opts.vehicle))
if not os.path.exists(vehicle_dir):
    print("vehicle directory (%s) does not exist" % (vehicle_dir,))
    sys.exit(1)

if not cmd_opts.hil:
    if cmd_opts.instance == 0:
        kill_tasks()

if cmd_opts.tracker:
    start_antenna_tracker(find_autotest_dir(), cmd_opts)

if cmd_opts.custom_location:
    location = cmd_opts.custom_location
    progress("Starting up at %s" % (location,))
else:
    location = find_location_by_name(find_autotest_dir(), cmd_opts.location)
    progress("Starting up at %s (%s)" % (location, cmd_opts.location))

if cmd_opts.hil:
    # (unlikely)
    run_in_terminal_window(find_autotest_dir(), "JSBSim", [os.path.join(find_autotest_dir(), "jsb_sim/runsim.py"), "--home", location, "--speedup=" + str(cmd_opts.speedup)])
else:
    if not cmd_opts.no_rebuild:  # i.e. we should rebuild
        do_build(vehicle_dir, cmd_opts, frame_infos)

    if cmd_opts.build_system == "waf":
        if cmd_opts.debug:
            binary_basedir = "build/sitl-debug"
        else:
            binary_basedir = "build/sitl"
        vehicle_binary = os.path.join(find_root_dir(), binary_basedir, frame_infos["waf_target"])
    else:
        vehicle_binary = os.path.join(vehicle_dir, cmd_opts.vehicle + ".elf")

    if not os.path.exists(vehicle_binary):
        print("Vehicle binary (%s) does not exist" % (vehicle_binary,))
        sys.exit(1)

    start_vehicle(vehicle_binary, find_autotest_dir(), cmd_opts, frame_infos, location)

if cmd_opts.delay_start:
    progress("Sleeping for %f seconds" % (cmd_opts.delay_start,))
    time.sleep(float(cmd_opts.delay_start))

start_mavproxy(cmd_opts, frame_infos)

sys.exit(0)
