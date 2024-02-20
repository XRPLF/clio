# Logging

Clio provides several logging options, which all are configurable via the config file. These are detailed in the following sections.

## `log_level`

The minimum level of severity at which the log message will be outputted by default. Severity options are `trace`, `debug`, `info`, `warning`, `error`, `fatal`. Defaults to `info`.

## `log_format`

 The format of log lines produced by Clio. Defaults to `"%TimeStamp% (%SourceLocation%) [%ThreadID%] %Channel%:%Severity% %Message%"`.

Each of the variables expands like so:

- `TimeStamp`: The full date and time of the log entry
- `SourceLocation`: A partial path to the c++ file and the line number in said file (`source/file/path:linenumber`)  
- `ThreadID`: The ID of the thread the log entry is written from
- `Channel`: The channel that this log entry was sent to
- `Severity`: The severity (aka log level) the entry was sent at
- `Message`: The actual log message

## `log_channels`

An array of JSON objects, each overriding properties for a logging `channel`.

> [!IMPORTANT]
> At the time of writing, only `log_level` can be overridden using this mechanism.

Each object is of this format:

```json
{
    "channel": "Backend",
    "log_level": "fatal"
}
```

If no override is present for a given channel, that channel will log at the severity specified by the global `log_level`.

The log channels that can be overridden are: `Backend`, `WebServer`, `Subscriptions`, `RPC`, `ETL` and `Performance`.

> [!NOTE]
> See [example-config.json](../example-config.json) for more details.

## `log_to_console`

Enable or disable log output to console. Options are `true`/`false`. This option defaults to `true`.

## `log_directory`

Path to the directory where log files are stored. If such directory doesn't exist, Clio will create it.

If the option is not specified, the logs are not written to a file.

## `log_rotation_size`

The max size of the log file in **megabytes** before it will rotate into a smaller file. Defaults to 2GB.

## `log_directory_max_size`

The max size of the log directory in **megabytes** before old log files will be deleted to free up space. Defaults to 50GB.

## `log_rotation_hour_interval`

The time interval in **hours** after the last log rotation to automatically rotate the current log file. Defaults to 12 hours.

> [!NOTE]
> Log rotation based on time occurs in conjunction with size-based log rotation. For example, if a size-based log rotation occurs, the timer for the time-based rotation will reset.

## `log_tag_style`

Tag implementation to use. Must be one of:

- `uint`: Lock free and threadsafe but outputs just a simple unsigned integer
- `uuid`: Threadsafe and outputs a UUID tag
- `none`: Doesn't use tagging at all
