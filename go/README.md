# Go in VMOD TinyKVM

Go is currently supported and should be able to run indefinitely despite being GC. See the AVIF transcoder example.


## Compute.JSON settings

Go requires special settings in JSON in order to be able to boot properly:

```json
{
	"go": {
		"uri": "https://.../goexample.tar.xz",
		"address_space": 1800,
		"remapping": ["0xC000000000", 256],
		"relocate_fixed_mmap": false,
		"max_memory": 64,
		"concurrency": 4,
		"storage": true
	}
}
```

1. Go must be given enough address space to cover its needs, even for small programs. Give the program at least 1800 MB. You can constrain memory down to very little working memory if you want, eg. 64MB.
2. Go needs a remapping for `0xC000000000`. It uses that area for GC, most likely. A remapping is just an allocation from your main address space that is moved to another area. Like stealing memory. It does not increase total memory.
3. Go needs the setting `relocate_fixed_mmap` set to false. It will keep retrying MMAP if any of its fixed mappings fail indefinitely. MMAP is not guaranteed to give you the area you want, so this is on the Go devs. But, this papers over the issue.
