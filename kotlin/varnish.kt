package varnish
import kotlinx.cinterop.*

public fun response(status: Short, ctype: String, content: String) {
    memScoped {
        val ctlen = ctype.length.toULong();
        val contlen = content.length.toULong();
        libvarnish.backend_response(status,
            ctype.cstr.ptr, ctlen, content.cstr.ptr, contlen);
    }
}

public fun set_cacheable(cached: Boolean, ttl: Float, grace: Float = 0.0f, keep: Float = 0.0f) {
    libvarnish.set_cacheable(cached, ttl, grace, keep);
}
