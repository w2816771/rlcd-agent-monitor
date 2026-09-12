#!/bin/zsh
set -euo pipefail

label="com.rlcd.monitor-host"
source_plist="${0:A:h}/${label}.plist"
installed_plist="${HOME}/Library/LaunchAgents/${label}.plist"
domain="gui/$(id -u)"

case "${1:-status}" in
  install)
    mkdir -p "${HOME}/Library/LaunchAgents"
    cp "${source_plist}" "${installed_plist}"
    launchctl bootout "${domain}" "${installed_plist}" 2>/dev/null || true
    launchctl bootstrap "${domain}" "${installed_plist}"
    launchctl kickstart -k "${domain}/${label}"
    echo "RLCD host service installed and started."
    ;;
  uninstall)
    launchctl bootout "${domain}" "${installed_plist}" 2>/dev/null || true
    rm -f "${installed_plist}"
    echo "RLCD host service removed."
    ;;
  restart)
    launchctl kickstart -k "${domain}/${label}"
    ;;
  logs)
    tail -n 50 /tmp/rlcd-host.log /tmp/rlcd-host.error.log 2>/dev/null || true
    ;;
  status)
    launchctl print "${domain}/${label}"
    ;;
  *)
    echo "Usage: $0 {install|uninstall|restart|status|logs}" >&2
    exit 2
    ;;
esac
