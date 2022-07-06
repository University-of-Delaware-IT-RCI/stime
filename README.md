# stime

When in the course of computational events, it becomes necessary for one program to bond with the outputs of Slurm, and to parse and thereby work with its temporal values, a decent programmer is impelled to write a utility to do so.

The `stime` utility links with the Slurm code library and uses the program's native time- and duration-parsing functionality to turn such strings into numerical values (in seconds).  Slurm's matching unparse functions can be used to return a numerical value to a string; various other formats are recognized by the program in parsing and unparsing directions.  Thus, a value parsed in one format can be displayed in another.

## Build

This program requires the installtion path of your Slurm software; the path to the Slurm source package associated with the Slurm software; and the path to the directory in which that source package was built:

```
$ SLURM_PREFIX=/opt/shared/slurm/current
$ cmake \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX="$SLURM_PREFIX" \
	-DSLURM_PREFIX="$SLURM_PREFIX" \
	-DSLURM_BUILD_DIR="${SLURM_PREFIX}/src/build" \
	 ..
$ make
$ make install
```

## Usage

```
$ stime --help
usage:

    stime {options} <value> {<value> ..}

  options:

    --help/-h                      show this information
    --quiet/-q                     emit no error messages
    --debug/-D                     emit extra info to stdout
    --reals/-r                     unparse seconds as float instead of integer
    --from/-F <format-id>          parse values in this format
                                   (default: slurm)
    --to/-T <format-id>            unparse values in this format
                                   (default: raw)
    --duration/-d                  values are interpreted as durations
    --timestamp/-t                 values are interpreted as timestamps

  formats:

    libc               strptime/strftime with locale-dependent times
    raw                Values are a number of seconds
    slurm              Slurm accepts a variety of timestamp and duration formats, 
                       please see the 'sbatch' man page
```

For example, turning an OS-native libc timestamp into a Slurm timestamp:

```
$ echo "$(date +%c)" | stime --timestamp --from=libc --to=slurm -
2022-07-06T20:12:31
```

The program defaults to Slurm inputs and raw outputs, so for a duration:

```
$ stime --duration 0-32:30:01
117001
```

Pipes allow easy chaining of results:

```
$ stime --duration 0-32:30:01 | stime --duration --from=raw --to=slurm -
1-08:30:01
```

with computation in the middle, perhaps (adding 5 minutes = 300 seconds):

```
$ stime --duration 0-32:30:01 | awk '{printf("%d",$1+300);}' |  stime --duration --from=raw --to=slurm -
1-08:35:01
```