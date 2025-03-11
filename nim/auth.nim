import varnish
import jwt, json, httpcore, strutils

const ec256PrivKey = """-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgevZzL1gdAFr88hb2
OF/2NxApJCzGCEDdfSp6VQO30hyhRANCAAQRWz+jn65BtOMvdyHKcvjBeBSDZH2r
1RTwjmYSi9R/zpBnuQ4EiMnCqfMPWiZqB4QdbAd0E7oH50VpuZ1P087G
-----END PRIVATE KEY-----"""
const ec256PubKey = """-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEEVs/o5+uQbTjL3chynL4wXgUg2R9
q9UU8I5mEovUf86QZ7kOBIjJwqnzD1omageEHWwHdBO6B+dFabmdT9POxg==
-----END PUBLIC KEY-----"""


proc sign(user: string): string =
    var token = toJWT(%*{
        "header": {
            "alg": "ES256",
            "typ": "JWT"
        },
        "claims": {
            "sub": "1234567890",
            "name": user,
            "iat": 1516239022
        }
    })
    token.sign(ec256PrivKey)
    return $token

proc verify(token: string): bool =
    try:
        let jwtToken = token.toJWT()
        return jwtToken.verify(ec256PubKey, ES256)
    except InvalidToken:
        return false

proc on_get(url: string, arg: string): (int, string, string) {.cdecl} =
    set_cacheable(false, 1.0f, 0.0f, 0.0f)

    if url == "/sign":
        return (200, "text/plain", sign("user"))

    let a = Http.get("Authorization")
    let tok = parseHeader(a).value[0]

    let verified = verify(tok)
    Http.set("X-Memory: " & formatSize(nim_workmem_current()) & "/" & formatSize(nim_workmem_max()))

    if verified:
        return (200, "text/plain", "Verified")
    else:
        return (401, "text/plain", "No access")

set_get_handler(on_get)
