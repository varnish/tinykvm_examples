import varnish.*

fun on_get(url: String) {
    set_cacheable(false, 0.1f)
    response(200, "text/plain", url + ": Hello, World!")
}

fun main(args: Array<String>) {
    println(args.contentToString())
}
