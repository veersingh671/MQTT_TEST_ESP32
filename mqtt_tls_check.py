import argparse
import socket
import ssl
import sys

# Defaults (override with command-line args)
HOST = "ec2-13-207-195-130.ap-south-1.compute.amazonaws.com"
PORT = 8883
CA_FILE = "root-ca.crt"
CLIENT_CERT = "test-device-2.crt"
CLIENT_KEY = "test-device-2.key"

def dump_peer_cert_raw(host, port):
    """Fetch the server certificate without verification and return:
    (pem, cert_dict, suggested_server_name)
    suggested_server_name is the first DNS SAN or the CommonName if SANs
    are not present.
    """
    print("=== Step 1: Fetching broker's server certificate (no verification) ===")
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    with socket.create_connection((host, port), timeout=10) as sock:
        # Use host as SNI to allow some servers to present the cert
        with ctx.wrap_socket(sock, server_hostname=host) as ssock:
            der_cert = ssock.getpeercert(binary_form=True)
            cert_dict = ssock.getpeercert()
            print("TLS version:", ssock.version())
            pem = ssl.DER_cert_to_PEM_cert(der_cert)
            print(pem)

            # Try to extract a suggested server name from SANs or CN.
            # Use ssl._ssl._test_decode_cert for a reliable parse of the PEM
            # (it expects a filename), but fall back to the raw dict if that
            # isn't available for some Python builds.
            suggested = None
            try:
                import tempfile, os
                with tempfile.NamedTemporaryFile('w', delete=False, suffix='.pem') as tf:
                    tf.write(pem)
                    tmpname = tf.name
                try:
                    decoded = ssl._ssl._test_decode_cert(tmpname)
                finally:
                    try:
                        os.unlink(tmpname)
                    except Exception:
                        pass
            except Exception:
                decoded = cert_dict

            san = decoded.get('subjectAltName', ()) or ()
            for typ, val in san:
                if typ == 'DNS':
                    suggested = val
                    break
            if not suggested:
                subj = decoded.get('subject', ()) or ()
                for rdn in subj:
                    for key, value in rdn:
                        if key.lower() in ('commonname', 'cn'):
                            suggested = value
                            break
                    if suggested:
                        break

            return pem, decoded, suggested

def verify_against_ca(host, port, ca_file, server_hostname):
    print("=== Step 2: Verifying broker cert against your root-ca.crt ===")
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.load_verify_locations(cafile=ca_file)
    ctx.check_hostname = True
    try:
        with socket.create_connection((host, port), timeout=10) as sock:
            with ctx.wrap_socket(sock, server_hostname=server_hostname) as ssock:
                print("SUCCESS: broker cert verified against", ca_file)
    except ssl.SSLCertVerificationError as e:
        print("FAILED: certificate verification error")
        print(e)
    except Exception as e:
        print("FAILED:", type(e).__name__, e)

def full_mutual_tls_handshake(host, port, ca_file, client_cert, client_key, server_hostname):
    print("=== Step 3: Full mutual TLS handshake (server verify + client cert) ===")
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    ctx.load_verify_locations(cafile=ca_file)
    ctx.load_cert_chain(certfile=client_cert, keyfile=client_key)
    ctx.check_hostname = True
    try:
        with socket.create_connection((host, port), timeout=10) as sock:
            with ctx.wrap_socket(sock, server_hostname=server_hostname) as ssock:
                print("SUCCESS: full mutual TLS handshake completed")
                print("Cipher:", ssock.cipher())
    except ssl.SSLCertVerificationError as e:
        print("FAILED at cert verification step")
        print(e)
    except Exception as e:
        print("FAILED:", type(e).__name__, e)

def parse_args():
    p = argparse.ArgumentParser(description="MQTT TLS diagnostic and mutual-TLS checker")
    p.add_argument('--host', default=HOST, help='Broker connect host (IP/DNS)')
    p.add_argument('--port', type=int, default=PORT, help='Broker port')
    p.add_argument('--ca', default=CA_FILE, help='Root CA file to verify broker cert')
    p.add_argument('--cert', default=CLIENT_CERT, help='Client certificate for mutual TLS')
    p.add_argument('--key', default=CLIENT_KEY, help='Client private key for mutual TLS')
    p.add_argument('--server-name', default=None, help='TLS server name to use for SNI/verification (defaults to SAN/CN from cert)')
    return p.parse_args()


def main():
    args = parse_args()

    try:
        pem, cert_dict, suggested = dump_peer_cert_raw(args.host, args.port)
    except Exception as e:
        print("Could not even connect to fetch server cert:", type(e).__name__, e)
        sys.exit(1)

    print()
    if args.server_name:
        server_name = args.server_name
        print("Using provided server-name for verification:", server_name)
    else:
        server_name = suggested or args.host
        print("Suggested server-name from certificate:", suggested)
        print("Using:", server_name)

    print()
    verify_against_ca(args.host, args.port, args.ca, server_name)
    print()
    full_mutual_tls_handshake(args.host, args.port, args.ca, args.cert, args.key, server_name)


if __name__ == "__main__":
    main()
