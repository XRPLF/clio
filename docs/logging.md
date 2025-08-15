# Logging

Clio provides several logging options, which all are configurable via the config file under the `log` section.
These are detailed in the following sections.

## `log.level`

The minimum level of severity at which the log message will be outputted by default. Severity options are `trace`, `debug`, `info`, `warning`, `error`, `fatal`. Defaults to `info`.

## `log.format`

The format of log lines produced by Clio using spdlog format patterns. Defaults to `"%Y-%m-%d %H:%M:%S.%f %^%3!l:%n%$ - %v"`.

Each of the variables expands like so:

- `%Y-%m-%d %H:%M:%S.%f`: The full date and time of the log entry with microsecond precision
- `%^`: Start color range
- `%3!l`: The severity (aka log level) the entry was sent at stripped to 3 characters
- `%n`: The logger name (channel) that this log entry was sent to
- `%$`: End color range
- `%v`: The actual log message

Some additional variables that might be useful:

- `%@`: A partial path to the C++ file and the line number in the said file (`src/file/path:linenumber`)
- `%t`: The ID of the thread the log entry is written from

For more information about spdlog format patterns, see: <https://github.com/gabime/spdlog/wiki/Custom-formatting>

## `log.is_async`

Whether spdlog is asynchronous or not.

## `log.channels`

An array of JSON objects, each overriding properties for a logging `channel`.

> [!IMPORTANT]
> At the time of writing, only `log.level` can be overridden using this mechanism.

Each object is of this format:

```json
{
  "channel": "Backend",
  "level": "fatal"
}
```

If no override is present for a given channel, that channel will log at the severity specified by the global `log.level`.

The log channels that can be overridden are: `Backend`, `WebServer`, `Subscriptions`, `RPC`, `ETL` and `Performance`.

> [!NOTE]
> See [example-config.json](../docs/examples/config/example-config.json) for more details.

## `log.enable_console`

Enable or disable log output to console. Options are `true`/`false`. This option defaults to `true`.

## `log.directory`

Path to the directory where log files are stored. If such directory doesn't exist, Clio will create it.

If the option is not specified, the logs are not written to a file.

## `log.rotation_size`

The max size of the log file in **megabytes** before it will rotate into a smaller file. Defaults to 2GB.

## `log.directory_max_files`

The max number of log files in the directory before old log files will be deleted to free up space. Defaults to 25.

## `log.tag_style`

Tag implementation to use. Must be one of:

- `uint`: Lock free and threadsafe but outputs just a simple unsigned integer
- `uuid`: Threadsafe and outputs a UUID tag
- `none`: Doesn't use tagging at all
