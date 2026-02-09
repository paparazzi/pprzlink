from setuptools import setup
from Cython.Build.Dependencies import cythonize
import os

dest_dir = os.getenv('DESTDIR')
if dest_dir is None:
    build_dir = 'build'
else:
    build_dir = os.path.normpath(os.path.join(dest_dir,'../build'))

setup(
    name="pprzlink",
    packages=["pprzlink"],
    version="2.0.0",
    ext_modules=cythonize([
        "src/pprzlink/ivy.py",
        "src/pprzlink/message.py",
        "src/pprzlink/messages_xml_map.py",
        "src/pprzlink/pprz_transport.py",
        "src/pprzlink/request_uid.py",
        "src/pprzlink/serial.py",
        "src/pprzlink/udp.py"
        ], language_level="3", build_dir=build_dir),
)
