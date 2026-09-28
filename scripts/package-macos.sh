#!/bin/bash
# Manual release gate. Never invoked by the evaluation-artifact workflow.
# Builds a signed/notarised installer in a fresh staging directory. Does not
# install plugins, publish a release or change the source build bundles.
set -euo pipefail

: "${MIRROR_RELEASE_APPROVED:?Set only after completing RELEASE_CHECKLIST.md}"
test "$MIRROR_RELEASE_APPROVED" = "yes"
: "${MIRROR_APPLICATION_IDENTITY:?Developer ID Application identity required}"
: "${MIRROR_INSTALLER_IDENTITY:?Developer ID Installer identity required}"
: "${MIRROR_NOTARY_PROFILE:?Existing notarytool keychain profile required}"
: "${MIRROR_LEGAL_DIRECTORY:?Absolute directory containing approved legal documents required}"
case "$MIRROR_APPLICATION_IDENTITY" in 'Developer ID Application:'*) ;; *) exit 2 ;; esac
case "$MIRROR_INSTALLER_IDENTITY" in 'Developer ID Installer:'*) ;; *) exit 2 ;; esac
case "$MIRROR_LEGAL_DIRECTORY" in /*) ;; *) exit 2 ;; esac

mirror_root="$(cd "$(dirname "$0")/.." && pwd)"
mirror_build="$mirror_root/build"
mirror_au="$mirror_build/Mirror_artefacts/Release/AU/MIRROR.component"
mirror_vst="$mirror_build/Mirror_artefacts/Release/VST3/MIRROR.vst3"
test -d "$mirror_au"
test -d "$mirror_vst"
for mirror_legal in EULA.txt PRIVACY.txt THIRD_PARTY_NOTICES.txt SUPPORT.txt; do
    test -s "$MIRROR_LEGAL_DIRECTORY/$mirror_legal"
done

# Gate on tests again, against this exact local build.
ctest --test-dir "$mirror_build" -C Release --output-on-failure
mirror_version="$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$mirror_au/Contents/Info.plist")"
test "$mirror_version" = "$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$mirror_vst/Contents/Info.plist")"
mirror_stage="$(mktemp -d "${TMPDIR:-/tmp}/mirror-release.XXXXXX")"
mirror_payload="$mirror_stage/payload"
mkdir -p "$mirror_payload/Library/Audio/Plug-Ins/Components" "$mirror_payload/Library/Audio/Plug-Ins/VST3"
ditto "$mirror_au" "$mirror_payload/Library/Audio/Plug-Ins/Components/MIRROR.component"
ditto "$mirror_vst" "$mirror_payload/Library/Audio/Plug-Ins/VST3/MIRROR.vst3"

for mirror_bundle in \
    "$mirror_payload/Library/Audio/Plug-Ins/Components/MIRROR.component" \
    "$mirror_payload/Library/Audio/Plug-Ins/VST3/MIRROR.vst3"; do
    lipo -verify_arch arm64 x86_64 "$mirror_bundle/Contents/MacOS/MIRROR"
    codesign --force --timestamp --options runtime --sign "$MIRROR_APPLICATION_IDENTITY" "$mirror_bundle"
    codesign --verify --deep --strict --verbose=2 "$mirror_bundle"
done

mirror_docs="$mirror_payload/Library/Application Support/MIRROR"
mkdir -p "$mirror_docs"
for mirror_legal in EULA.txt PRIVACY.txt THIRD_PARTY_NOTICES.txt SUPPORT.txt; do
    cp "$MIRROR_LEGAL_DIRECTORY/$mirror_legal" "$mirror_docs/$mirror_legal"
done
cp "$mirror_root/QUICK_START.md" "$mirror_docs/QUICK_START.md"

mirror_package="$mirror_stage/MIRROR-$mirror_version.pkg"
pkgbuild --root "$mirror_payload" --identifier com.louisgabriel.mirror.installer \
    --version "$mirror_version" --install-location / --ownership recommended \
    --sign "$MIRROR_INSTALLER_IDENTITY" "$mirror_package"
pkgutil --check-signature "$mirror_package"
xcrun notarytool submit "$mirror_package" --keychain-profile "$MIRROR_NOTARY_PROFILE" --wait
xcrun stapler staple "$mirror_package"
xcrun stapler validate "$mirror_package"
spctl --assess --type install --verbose=2 "$mirror_package"
shasum -a 256 "$mirror_package" > "$mirror_package.sha256"
echo "Staged installer (not published): $mirror_package"
echo "Retain this staging directory until clean-Mac installation and rollback tests pass."
