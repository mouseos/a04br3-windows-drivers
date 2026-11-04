"""One-use, exact machine-readable offline completion signal."""
import re
import cv2
import numpy as np
from PIL import Image


def payload(token):
    if not re.fullmatch(r'[0-9a-f]{32}', token):
        raise ValueError('Expected 128-bit lowercase hex token')
    return ('A04-MMA-OFFLINE-V1\nTOKEN=' + token + '\nMOUNTVOL_EXIT=0\n'
            'MMA8652_OFFLINE_STAGING_FINISHED')


def make_image(token, dest):
    data = payload(token)
    qr = cv2.QRCodeEncoder_create().encode(data)
    if qr is None or qr.ndim != 2:
        raise ValueError('QR encoding failed')
    qr = cv2.copyMakeBorder(qr, 4, 4, 4, 4, cv2.BORDER_CONSTANT, value=255)
    scale = max(1, min(10, 700 // max(qr.shape)))
    qr = cv2.resize(qr, None, fx=scale, fy=scale, interpolation=cv2.INTER_NEAREST)
    if cv2.QRCodeDetector().detectAndDecode(qr)[0] != data:
        raise ValueError('QR round-trip failed')
    Image.fromarray(np.asarray(qr, dtype=np.uint8)).convert('RGB').save(dest, 'BMP')
