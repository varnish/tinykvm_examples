set -e -u
FPATH="$PWD"
PATTERN="\.ts$"
URL="http://127.0.0.1:8080/invdeno"

inotifywait -q --format '%f' -m -r -e close_write $FPATH \
    | grep --line-buffered $PATTERN \
    | xargs -I{} -r sh -c "curl -D - $URL && echo"
