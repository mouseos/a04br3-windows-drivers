#!/usr/bin/env bash
# Generate a disposable *test* identity and sign an ARM32 diagnostic helper.
set -euo pipefail
if [[ $# != 3 ]]; then echo 'usage: create-audit-test-signer.sh UNSIGNED_EXE OSSLSIGNCODE NEW_DIRECTORY' >&2; exit 2; fi
root=$(cd "$(dirname "$0")/../.." && pwd -P); workspace=$(cd "$root/../.." && pwd -P)
exe=$(realpath "$1"); sign=$(realpath "$2"); out=$(realpath -m "$3")
[[ $exe == "$workspace"/* && $sign == "$workspace"/* && $out == "$workspace"/* && -f $exe && -x $sign && ! -e $out ]] || exit 2
umask 077; mkdir -m 700 "$out"; mkdir "$out/newcerts"; : > "$out/index.txt"; printf '01\n' > "$out/serial"
cat > "$out/openssl.cnf" <<CONFIG
[ ca ]
default_ca = local_ca
[ local_ca ]
dir = $out
database = \$dir/index.txt
new_certs_dir = \$dir/newcerts
serial = \$dir/serial
default_md = sha256
policy = policy_any
x509_extensions = signing
[ policy_any ]
commonName = supplied
[ signing ]
basicConstraints = critical,CA:TRUE
keyUsage = critical,digitalSignature,keyCertSign
extendedKeyUsage = codeSigning
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always
CONFIG
/usr/bin/openssl req -new -newkey rsa:2048 -nodes -keyout "$out/key.pem" -out "$out/request.pem" -subj '/CN=A04BR3 Hive Audit Test' >/dev/null 2>&1
/usr/bin/openssl ca -batch -selfsign -config "$out/openssl.cnf" -keyfile "$out/key.pem" -in "$out/request.pem" -startdate 20120101000000Z -enddate 20400101000000Z -out "$out/cert.pem" >/dev/null 2>&1
/usr/bin/openssl x509 -in "$out/cert.pem" -outform DER -out "$out/cert.cer"
"$sign" sign -certs "$out/cert.pem" -key "$out/key.pem" -n 'A04BR3 Hive Audit Test' -in "$exe" -out "$out/audit-hive-signed.exe"
"$sign" verify -CAfile "$out/cert.pem" -in "$out/audit-hive-signed.exe"
sha256sum "$exe" "$out/cert.cer" "$out/audit-hive-signed.exe" > "$out/hashes.txt"
echo 'Test identity only; protect key.pem and do not include it in deliverables.'
