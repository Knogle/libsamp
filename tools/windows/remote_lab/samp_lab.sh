#!/usr/bin/env bash

set -euo pipefail

lab_host="${SAMP_LAB_HOST:-sshadmin@192.168.3.180}"
lab_key="${SAMP_LAB_KEY:-${HOME}/.ssh/id_default}"
remote_root='C:\samp-test'
remote_scripts="${remote_root}\\scripts"
ssh_options=(-F /dev/null -i "$lab_key" -o BatchMode=yes -o IdentitiesOnly=yes)

usage() {
    printf '%s\n' \
        "Usage:" \
        "  $0 ping" \
        "  $0 screenshot [label]" \
        "  $0 key <ENTER|ESCAPE|SPACE|UP|DOWN|LEFT|RIGHT|MODE|CLASS|KILL|QUIT|SFA|LVA|AA|ACTORS|ACTORSOFF|RPC175EDGE|RPC175EDGEOFF|RPC175RAW|RPC176RAW|RPC178EDGE|RPC178EDGEOFF> [label]" \
        "  $0 click <window-x> <window-y> [label]" \
        "  $0 start <scenario> [samp|gta] [server-host] [server-port] [nickname] [favorite-index]" \
        "  $0 collect" \
        "  $0 stop" \
        "  $0 logcheck" \
        "  $0 probe-profile <passive|no-hooks|asset-paths|custom-object-heavy|textdraw|textdraw-verbose|textdraw-render|font5|actor|actor-heavy|rpc-gap|dialog-menu>" \
        "  $0 overlay-profile <bypass|shadow|replace>" \
        "  $0 overlay-kill <on|off>" \
        "  $0 validate <local-samp.dll>" \
        "  $0 deploy <local-samp.dll> <label>" \
        "  $0 deploy-probe <local-samp_probe.asi> <label>" \
        "  $0 deploy-overlay <local-samp_re.asi> <label>" \
        "  $0 fetch-run <run-id> <local-directory>"
}

require_safe_name() {
    local value="$1"
    local field="$2"
    if [[ ! "$value" =~ ^[A-Za-z0-9_.-]+$ ]]; then
        printf 'Invalid %s: %s\n' "$field" "$value" >&2
        exit 2
    fi
}

run_agent_action() {
    local action="$1"
    shift
    ssh "${ssh_options[@]}" "$lab_host" \
        powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
        -File "${remote_scripts}\\Submit-SampTestCommand.ps1" \
        -Root "$remote_root" -Action "$action" "$@"
}

upload_candidate() {
    local candidate="$1"
    if [[ ! -f "$candidate" ]]; then
        printf 'Candidate DLL not found: %s\n' "$candidate" >&2
        exit 2
    fi
    scp "${ssh_options[@]}" "$candidate" "$lab_host:C:/samp-test/incoming/samp.dll"
}

upload_asi_candidate() {
    local candidate="$1"
    local staged_name="$2"
    if [[ ! -f "$candidate" ]]; then
        printf 'Candidate ASI not found: %s\n' "$candidate" >&2
        exit 2
    fi
    case "$staged_name" in
        samp_probe.asi|samp_re.asi) ;;
        *) printf 'Unsupported staged ASI name: %s\n' "$staged_name" >&2; exit 2 ;;
    esac
    scp "${ssh_options[@]}" "$candidate" "$lab_host:C:/samp-test/incoming/$staged_name"
}

command="${1:-}"
case "$command" in
    ping)
        run_agent_action ping
        ;;
    screenshot)
        label="${2:-manual}"
        require_safe_name "$label" label
        run_agent_action screenshot -Label "$label"
        ;;
    key)
        key="${2:-}"
        label="${3:-key}"
        case "$key" in
            ENTER|ESCAPE|SPACE|UP|DOWN|LEFT|RIGHT|MODE|CLASS|KILL|QUIT|SFA|LVA|AA|ACTORS|ACTORSOFF|RPC175EDGE|RPC175EDGEOFF|RPC175RAW|RPC176RAW|RPC178EDGE|RPC178EDGEOFF) ;;
            *) printf 'Unsupported key: %s\n' "$key" >&2; exit 2 ;;
        esac
        require_safe_name "$label" label
        run_agent_action input -InputMode key -InputKey "$key" -Label "$label"
        ;;
    click)
        x="${2:-}"
        y="${3:-}"
        label="${4:-click}"
        if [[ ! "$x" =~ ^[0-9]+$ ]] || [[ ! "$y" =~ ^[0-9]+$ ]]; then
            printf 'Click coordinates must be non-negative integers.\n' >&2
            exit 2
        fi
        require_safe_name "$label" label
        run_agent_action input -InputMode click -InputX "$x" -InputY "$y" -Label "$label"
        ;;
    start)
        scenario="${2:-}"
        target="${3:-samp}"
        server_host="${4:-}"
        server_port="${5:-0}"
        nickname="${6:-WinDebug}"
        favorite_index="${7:-3}"
        require_safe_name "$scenario" scenario
        if [[ "$target" != samp && "$target" != gta ]]; then
            printf 'Target must be samp or gta.\n' >&2
            exit 2
        fi
        if [[ -n "$server_host" ]]; then
            if [[ ! "$server_host" =~ ^[A-Za-z0-9.-]+$ ]]; then
                printf 'Invalid server host: %s\n' "$server_host" >&2
                exit 2
            fi
            if [[ ! "$server_port" =~ ^[0-9]+$ ]] || ((server_port < 1 || server_port > 65535)); then
                printf 'Invalid server port: %s\n' "$server_port" >&2
                exit 2
            fi
            require_safe_name "$nickname" nickname
            if [[ "$target" == samp ]] && { [[ ! "$favorite_index" =~ ^[0-9]+$ ]] || ((favorite_index > 100)); }; then
                printf 'Invalid favorite index: %s\n' "$favorite_index" >&2
                exit 2
            fi
        fi
        start_args=(
            -Scenario "$scenario"
            -Target "$target"
        )
        if [[ -n "$server_host" ]]; then
            start_args+=(
                -ServerHost "$server_host"
                -ServerPort "$server_port"
                -Nickname "$nickname"
            )
            if [[ "$target" == samp ]]; then
                start_args+=( -FavoriteIndex "$favorite_index" )
            fi
        fi
        run_agent_action start "${start_args[@]}"
        ;;
    collect|stop|logcheck)
        run_agent_action "$command"
        ;;
    probe-profile)
        profile="${2:-}"
        case "$profile" in
            passive|no-hooks|asset-paths|custom-object-heavy|textdraw|textdraw-verbose|textdraw-render|font5|actor|actor-heavy|rpc-gap|dialog-menu) ;;
            *) printf 'Unsupported probe profile: %s\n' "$profile" >&2; exit 2 ;;
        esac
        ssh "${ssh_options[@]}" "$lab_host" \
            powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
            -File "${remote_scripts}\\Set-SampProbeProfile.ps1" \
            -Root "$remote_root" -Profile "$profile"
        ;;
    overlay-profile)
        profile="${2:-}"
        case "$profile" in
            bypass|shadow|replace) ;;
            *) printf 'Unsupported overlay profile: %s\n' "$profile" >&2; exit 2 ;;
        esac
        ssh "${ssh_options[@]}" "$lab_host" \
            powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
            -File "${remote_scripts}\\Set-SampReProfile.ps1" \
            -Root "$remote_root" -Profile "$profile"
        ;;
    overlay-kill)
        state="${2:-}"
        case "$state" in
            on|off) ;;
            *) printf 'Unsupported overlay kill-switch state: %s\n' "$state" >&2; exit 2 ;;
        esac
        ssh "${ssh_options[@]}" "$lab_host" \
            powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
            -File "${remote_scripts}\\Set-SampReKillSwitch.ps1" \
            -Root "$remote_root" -State "$state"
        ;;
    validate)
        candidate="${2:-}"
        upload_candidate "$candidate"
        ssh "${ssh_options[@]}" "$lab_host" \
            powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
            -File "${remote_scripts}\\Deploy-SampDll.ps1" \
            -Root "$remote_root" -Candidate "${remote_root}\\incoming\\samp.dll" -ValidateOnly
        ;;
    deploy)
        candidate="${2:-}"
        label="${3:-}"
        require_safe_name "$label" label
        upload_candidate "$candidate"
        ssh "${ssh_options[@]}" "$lab_host" \
            powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
            -File "${remote_scripts}\\Deploy-SampDll.ps1" \
            -Root "$remote_root" -Candidate "${remote_root}\\incoming\\samp.dll" -Label "$label"
        ;;
    deploy-probe|deploy-overlay)
        candidate="${2:-}"
        label="${3:-}"
        require_safe_name "$label" label
        if [[ "$command" == deploy-probe ]]; then
            asi_name="samp_probe.asi"
        else
            asi_name="samp_re.asi"
        fi
        upload_asi_candidate "$candidate" "$asi_name"
        ssh "${ssh_options[@]}" "$lab_host" \
            powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass \
            -File "${remote_scripts}\\Deploy-SampAsi.ps1" \
            -Root "$remote_root" -Candidate "${remote_root}\\incoming\\${asi_name}" \
            -Name "$asi_name" -Label "$label"
        ;;
    fetch-run)
        run_id="${2:-}"
        destination="${3:-}"
        require_safe_name "$run_id" run-id
        if [[ -z "$destination" ]]; then
            usage >&2
            exit 2
        fi
        mkdir -p "$destination"
        scp "${ssh_options[@]}" -r "$lab_host:C:/samp-test/runs/$run_id" "$destination/"
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
