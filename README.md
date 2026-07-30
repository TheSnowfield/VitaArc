# VitaArc

Run Arcaea on your HENkaku PSVita.  
This is a cross-system ABI bridge to simulate the android environment of Arcaea in the SCE system.

`Arcaea` is a mobile game who developed by Lowiro.  
This project is not affiliated with Lowiro or Arcaea.

## Hint
This project now **under development**.  
You may not get the game running correctly.

## Get started
### Install
 - Download the `VitaArc.vpk` from the release page.  
 (random laughing) ~~cause now there's no release exists. XD~~
 - Connect your PSVita to PC via VitaShell and copy `VitaArc.vpk` into it.
 - Install VitaArc.

### Package game resources

Place the official APK in `third-party/arcaea/` before building. The packaging
script imports the selected APK into `package/` in the VPK. At runtime these
read-only files are available below `app0:package/`; no manual extraction of
the APK to the memory card is required.

Writable files such as logs and persistent game data remain below
`ux0:vitaarc/`.

### Continuous integration

GitHub Actions and GitLab CI build a base `VitaArc.vpk` with the official
`vitasdk/vitasdk` container and publish it as a pipeline artifact. The CI
artifact intentionally excludes the proprietary Arcaea APK resources.

To build the same base VPK locally:

```sh
VITAARC_PACKAGE_APK=OFF bash build.sh
```

Local builds package an APK by default. If multiple APK versions are present,
select one explicitly:

```sh
bash build.sh -DARCAEA_VERSION=1.6.1c
```

### Play & PM
 - Click VitaArc bubble.
 - Wait for the game load.
 - Try your best to `PM` all of the songs.
