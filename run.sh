if [ -e /etc/os-release ] ; then
    distro=$(awk -F= '$1 == "ID" {print $2}' /etc/os-release | tr -d \")
    version=$(awk -F= '$1 == "VERSION_ID" {print $2}' /etc/os-release | tr -d \")
    version=${version%.*}
elif type lsb_release >/dev/null 2>&1 ; then
    distro=$(lsb_release -i -s)
else
    echo "Unknown distro release system" ; exit 1
fi
# Convert to lowercase
distro=$(printf '%s\n' "$distro" | LC_ALL=C tr '[:upper:]' '[:lower:]')

case "$distro" in
    alpine*)    DISTRO=alpine ;;
    debian*)    DISTRO=debian-$version ;;
    centos*)    DISTRO=rocky-8 ;;
    fedora*)    DISTRO=fedora-$version ;;
    mint*)      DISTRO=ubuntu-20 ;;
    rocky*)     DISTRO=rocky-$version ;;
    ubuntu*)    DISTRO=ubuntu-$version ;;
    *)  echo "Unknown distro: '$distro'" ; exit 1 ;;
esac

echo "Distro: $DISTRO (Version: $version)"
NTMP=/tmp/varnishd
VMOD=$PWD/vmod/$DISTRO
rm -rf $NTMP
mkdir -p $NTMP
varnishd -a :8080 -f $PWD/kvm.vcl -n $NTMP -p vmod_path=$VMOD -F
