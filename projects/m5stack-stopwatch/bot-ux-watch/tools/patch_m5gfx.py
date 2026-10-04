Import("env")

import hashlib
import json
import subprocess
from pathlib import Path


M5GFX_REVISION = "d91077b9a607b59404e4e4a49f775c792bfae382"
M5GFX_FILES = {
    "src/M5GFX.cpp": (
        "df1b2e3882c45d1aa7743f6f30081fc753c609cdf38f352143f7f3bc3738514e",
        "494903aa46742226bdd9a8b23fa65b4cb27784ddfeb20cbff96efdcb0be92d0c",
    ),
    "src/lgfx/v1/panel/Panel_AMOLED.cpp": (
        "8b71b03b51693a14890872e6918723e191976200fdeb9658d203363c862be4bf",
        "e95141b3ca53edd95a3d205cb8d9cb0063139fb35cb007959b02c4c76d18c2b4",
    ),
    "src/lgfx/v1/panel/Panel_AMOLED.hpp": (
        "d8cdc0b76ae951a8f2ce73f8128ecd40aae237c7485646fd218d24a91198f96e",
        "38a18ae04140c6306c79578d79f3c4469717027f9b16cdb5aec20be93add9ad5",
    ),
    "src/lgfx/v1/touch/Touch_CSTxxx.cpp": (
        "8f2d4e2dd56659ecd684471fdb4622a11f51c3691f7f9c53e4f93641e9cb3ead",
        "492f92a8c5f1051a2f1d96b372b71475596ffc48dba39b17746ec331312d3544",
    ),
    "src/lgfx/v1/touch/Touch_CSTxxx.hpp": (
        "4a89e6dd2fe8d81dec09a7b3907edf098d841b0786cc8432dec37195d69c1040",
        "52b919b32d61b91c8ba81c273fa878a5d84c2381e1964b7f65e703ce230c5211",
    ),
}
M5UNIFIED_REVISION = "8530f5377d782e4a25a6c482de2e71c3f75ca8eb"
M5UNIFIED_FILES = {
    "src/utility/power/M5PM1_Class.hpp": (
        "3e55b7fac40db554cd511ae2d178b16713788e593b8cc570b104b93ba16a4016",
        "5a9c23565340b3a83367ffefc4234d19904afc9dedcc6b28a3599f7bdd1593d6",
    ),
    "src/utility/power/M5PM1_Class.cpp": (
        "5ffe085a11d139a2e1ea16b3499e23c97af82020e09aa2275ca021e8646e6f10",
        "18650e10308e4ecff5b2b65e4ec60e38d9014077897e1ad8fd204460fb6dc270",
    ),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


deps = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV")


def find_dependency(name, revision):
    candidates = []
    for directory in deps.glob(name + "*"):
        manifest = directory / "library.json"
        if not manifest.is_file() or json.loads(manifest.read_text()).get("name") != name:
            continue
        try:
            actual = subprocess.check_output(
                ["git", "rev-parse", "HEAD"], cwd=str(directory), text=True
            ).strip()
        except subprocess.CalledProcessError:
            continue
        if actual == revision:
            candidates.append(directory)
    if len(candidates) != 1:
        raise RuntimeError("Expected exactly one installed %s dependency, found %d" % (name, len(candidates)))
    return candidates[0]


def apply_verified(name, revision, files, patch_name, description):
    library = find_dependency(name, revision)
    state = [digest(library / filename) for filename in files]
    original = [hashes[0] for hashes in files.values()]
    patched = [hashes[1] for hashes in files.values()]
    if state == original:
        patch = Path(env.subst("$PROJECT_DIR")) / "patches" / patch_name
        subprocess.run(["git", "apply", "--unidiff-zero", "--check", str(patch)], cwd=str(library), check=True)
        subprocess.run(["git", "apply", "--unidiff-zero", str(patch)], cwd=str(library), check=True)
        state = [digest(library / filename) for filename in files]
        if state != patched:
            raise RuntimeError("%s patch produced unexpected content" % name)
        print("Applied verified " + description)
    elif state == patched:
        print("Verified existing " + description)
    else:
        raise RuntimeError("Refusing to patch modified or unexpected %s sources" % name)


apply_verified("M5GFX", M5GFX_REVISION, M5GFX_FILES,
               "m5gfx-stopwatch-cst820.patch", "M5GFX StopWatch CST820 patch")
apply_verified("M5Unified", M5UNIFIED_REVISION, M5UNIFIED_FILES,
               "m5unified-stopwatch-power-button.patch", "M5Unified StopWatch power-button patch")
