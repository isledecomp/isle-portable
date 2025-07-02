from collections import defaultdict
import hashlib, binascii, sys

class EmdLibrary:
    name: str
    stubfile: str
    nidsuffix: str
    libnamenid: int
    functions: list[str]
    variables: list[str]
    
    def __init__(self):
        self.name = ""
        self.version = ""
        self.stubfile = ""
        self.nidsuffix = ""
        self.libnamenid = ""
        self.functions = []
        self.variables = []


def loadEmd(filename: str):
    with open(filename, "r") as f:
        lines = f.read().splitlines()
    libraries = defaultdict(EmdLibrary)
    for line in lines:
        if not line:
            continue
        sp = line.split(" ")
        for i in range(0,len(sp), 2):
            key = sp[i].replace(":", "")
            value = sp[i+1]
            if key == "//":
                break
            match key:
                case "Library":
                    library = libraries[value]
                    library.name = value
                case "function":
                    ent = [value, ""]
                    library.functions.append(ent)
                case "variable":
                    ent = [value, ""]
                    library.variables.append(ent)
                case "nidvalue":
                    ent[1] = value.replace("\"", "")
                case "nidsuffix":
                    library.nidsuffix = value
                case "version":
                    library.version = value
                case "libnamenid":
                    library.libnamenid = value
                case "emd" | "attr" | "stubfile":
                    pass
                case _:
                    print("unk key", key, value)
    return list(libraries.values())

def nid(name: str, suffix: str):
    return "0x"+binascii.b2a_hex(bytes(reversed(hashlib.sha1((name+suffix).encode()).digest()[:4]))).decode()

def main():
    outyml = sys.argv[1]
    emd_filename = sys.argv[2]
    emds = loadEmd(emd_filename)

    with open(outyml, "w") as f:
        def out(*args):    
            print(*args, file=f)
        out("version: 2")
        out("firmware: 3.60")
        out("modules:")
        for library in emds:
            out(f"  {library.name}:")
            out(f"    libraries:")
            out(f"      {library.name}:")
            out(f"        kernel: false")
            out(f"        nid: {library.libnamenid}")
            out(f"        functions:")
            for func, nidvalue in library.functions:
                out(f"          {func}: {nidvalue or nid(func, library.nidsuffix)}")
            if library.variables:
                out(f"        variables:")
                for var, nidvalue in library.variables:
                    out(f"          {var}: {nidvalue or nid(var, library.nidsuffix)}")

main()
