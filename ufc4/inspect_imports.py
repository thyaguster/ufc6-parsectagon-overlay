import glob
import os
import pefile

root = r'E:\Jogos\PORTHEON\games\UFC4\bin'
for path in glob.glob(root + r'\*.exe') + glob.glob(root + r'\*.dll'):
    try:
        pe = pefile.PE(path, fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_IMPORT']])
        for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
            if b'WINMM' in entry.dll.upper():
                names = [item.name.decode(errors='replace') if item.name else str(item.ordinal) for item in entry.imports]
                print(os.path.basename(path), names)
    except Exception as error:
        print(os.path.basename(path), type(error).__name__, error)
