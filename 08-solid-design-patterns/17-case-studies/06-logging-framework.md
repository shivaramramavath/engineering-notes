# Case Study 06: Logging Framework

**Patterns Used**: **Chain of Responsibility** (Log level filtering + Appender chain), **Strategy** (`JsonFormatter`, `PlainTextFormatter`), **Async Producer–Consumer Queue** (non-blocking log flush with bounded drop/backpressure).

## Key Design Insight
Decouple **where** logs are created (`Logger.info`), **how** they are formatted (`Formatter` Strategy), and **where** they are written (`ConsoleAppender`, `FileAppender`, `OTelAppender` Sink)—buffering disk/network writes via a bounded async queue so slow disk I/O never stalls request threads.
