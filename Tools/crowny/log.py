import sys

_INFO = "\033[36m"
_WARN = "\033[33m"
_ERROR = "\033[31m"
_RESET = "\033[0m"
_COLOR = sys.stdout.isatty()


def _paint(color, text):
    if _COLOR:
        return f"{color}{text}{_RESET}"
    return text


def info(message):
    print(_paint(_INFO, "[crowny]") + f" {message}")


def warn(message):
    print(_paint(_WARN, "[crowny]") + f" {message}")


def error(message):
    print(_paint(_ERROR, "[crowny]") + f" {message}", file=sys.stderr)


def fail(message):
    error(message)
    raise SystemExit(1)
