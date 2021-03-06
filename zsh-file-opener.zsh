alias ${_ZSH_FILE_OPENER_CMD:-f}='file_opener'
alias -g "${(U)_ZSH_FILE_OPENER_CMD}${(U)_ZSH_FILE_OPENER_CMD}"=' |& file_opener'
_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES=${_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES:-"otf,ttf,iso"}
_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS=${_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS:-"mkv,mp4,movs,mp3,avi,mpg,m4v,oga,m4a,m4b,opus,wmv,mov,wav"}
_ZSH_FILE_OPENER_BOOK_FORMATS=${_ZSH_FILE_OPENER_BOOK_FORMATS:-"pdf,epub,djvu"}
_ZSH_FILE_OPENER_PICTURE_FORMATS=${_ZSH_FILE_OPENER_PICTURE_FORMATS:-"jpeg,jpg,png,webp,svg,gif,bmp,tif,tiff,psd"}
_ZSH_FILE_OPENER_WEB_FORMATS=${_ZSH_FILE_OPENER_WEB_FORMATS:-"html,mhtml"}
_ZSH_FILE_OPENER_ARCHIVE_FORMATS=${_ZSH_FILE_OPENER_ARCHIVE_FORMATS:-"gz,tgz,bz2,tbz,tbz2,xz,txz,zma,tlz,zst,tzst,tar,lz,gz,bz2,xz,lzma,z,zip,war,jar,sublime-package,ipsw,xpi,apk,aar,whl,rar,rpm,7z,deb,zs"}
zstyle ':completion:*:*:file_opener:*:*' file-patterns '^*.(${_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES//,/|})' '*(D):all-files'
zstyle ':completion:*:*:file_opener:*:*' ignore-line other

if [[ $SSH_TTY ]]; then
    export EDITOR='rmate -w'
    export VISUAL='rmate -w'
    _ZSH_FILE_OPENER_EXCLUDE_SUFFIXES+=",$_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS,$_ZSH_FILE_OPENER_BOOK_FORMATS,$_ZSH_FILE_OPENER_PICTURE_FORMATS"
    alias -g SS=' |& rmate -'

    if [[ ! -f "$HOME/.local/bin/rmate" ]]; then
        curl https://raw.githubusercontent.com/aurora/rmate/master/rmate > "$HOME/.local/bin/rmate"
        chmod +x "$HOME/.local/bin/rmate"
    fi

    if type doas > /dev/null 2>&1; then
        root_cmd() { doas ${$(where rmate)[1]} "$@" }
    elif type sudo > /dev/null 2>&1; then
        root_cmd() { sudo ${$(where rmate)[1]} "$@" }
    else
        root_cmd() { echo "No root escalation command like sudo or doas found. Try su instead." }
    fi

    _docs_opener() {
        for doc in ${docs[@]}; do
            if [[ -w "${${doc:a}%/*}" ]]; then
                rmate ${(Q)doc}
            else
                root_cmd ${(Q)doc}
            fi
        done
    }
else
    if [[ $WAYLAND_DISPLAY ]]; then
    alias -g SS=' |& subl -'
        _docs_opener() {
            swaymsg -q -- [app_id=^sublime_text$] focus, exec \'/opt/sublime_text/sublime_text ${docs}\' ||\
            swaymsg -q -- exec /opt/sublime_text/sublime_text, exec \'/opt/sublime_text/sublime_text ${docs}\'
        }
    else
        _docs_opener() {
            [[ -z $EDITOR ]] && print '$EDITOR not set' && return 1
            $EDITOR ${docs}
        }
    fi
fi

file_opener() {
    typeset -aU arcs movs pdfs pics webs docs dirs batstatus vscode disabled array
    local ret arg

    if [[ -t 0 ]]; then
        [[ -z "$@" ]] && cd > /dev/null 2>&1 && return 0
        for arg in "$@"; do
            case "${arg}" in
                (-c|--create) local _create=true ;;
                (-f|--force)  local _ZSH_FILE_OPENER_EXCLUDE_SUFFIXES="" ;;
                (-t|--text)   local _OPEN_IN_TEXT_EDITOR=true ;;
                (*) array+=("$arg") ;;
            esac
        done
    else
        # turn stdin into args -- useful in combination with grep --files-with-matches
        while read -r arg ; do
            array+=("$arg")
        done
    fi

    for file in "${array[@]}"; do
        if [[ ! -r "${file}" ]]; then
            if [[ ! -z $_create ]]; then
                mkdir -p "${${file:a}%/*}"
            # it is only an error if we cannot read the containing dir, OR if we cannot read the file
            elif [[ -e "${file%/}" ]] || [[ ! -r "${${file:a}%/*}" ]]; then
                local -a dir_parts=( ${(s[/])${file:a}} )
                local assembly="/"
                for num in {1..${#dir_parts[@]}}; do
                    assembly+="${dir_parts[$num]}"
                    if [[ ! -r $assembly ]]; then
                        print "Permission denied: $(_colorizer $assembly)"
                        ret=1
                        continue 2
                    fi
                    assembly+="/"
                done
            fi
        fi
        if [[ -n $_OPEN_IN_TEXT_EDITOR ]]; then
            docs+=("${file:A:q}")
            continue
        elif [[ -d ${file} ]]; then
            dirs+=("$file")
            continue
        else
            case "${file:e:l}" in
                (${~_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES//,/|})
                    disabled+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_ARCHIVE_FORMATS//,/|})
                    arcs+=(${file:a})
                    [[ "${#@}" -eq 2 ]] && { local explicit_extract_location="$2"; break } ;;
                (${~_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS//,/|})
                    movs+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_BOOK_FORMATS//,/|})
                    swaymsg -q "[app_id=\"^org.pwmt.zathura$\" title=\"^${(q)file##*/}\ \[\"] focus" || pdfs+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_PICTURE_FORMATS//,/|})
                    pics+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_WEB_FORMATS//,/|})
                    webs+=("${file:A:q}") ;;
                (ipynb)
                    vscode+=("${file:A:q}") ;;
                (*)
                    [[ "${#@}" -eq 2 ]] && [ $2 -gt 0 2>/dev/null ] && docs+=("${file:A:q}":$2) && break
                    docs+=("${file:A:q}") ;;
            esac
        fi
    done

    [[ ${dirs} ]] && {
        if [[ ${#dirs} -eq 1 ]]; then
            cd "$dirs" && ret=${ret:-0}
        else
            local -aU color_dirs
            for dir in ${dirs}; do
                color_dirs+="$(_colorizer $dir)"
            done
            print "Cannot enter multiple directories: ${color_dirs}"
            ret=1
        fi
    }

    [[ ${disabled} ]] && {
        local -a disabled_files
        local disabled_file joined
        for disabled_file in ${disabled}; do
            disabled_files+="$(_colorizer "$disabled_file")"
        done
        printf -v joined '%s\e[32m\e[1m,\e[0m ' "${disabled_files[@]}"
        print "File opener is disabled for: ${joined}"
        ret=1
    }

    [[ ${movs} ]] && {
        if pgrep -x mpv > /dev/null 2>&1; then
            [[ ! -S /tmp/mpvsocket ]] && print "mpvsocket not found" && ret=1
            for movie in ${movs[@]}; do
                print "loadfile ${(qq)movie} append" | socat - /tmp/mpvsocket
                notify-send.sh "${movie##*/}" "Playing next???" --default-action="swaymsg -q '[app_id=^mpv$] focus'"
            done
        else
            case "$(aplay -l)" in
            *": BT600 ["*)
                local audio='--audio-device=alsa/iec958:CARD=BT600,DEV=0'
                ;;
            *": BT700 ["*)
                local audio='--audio-device=alsa/iec958:CARD=BT700,DEV=0'
                ;;
             *": Audio ["*)
                local audio='--audio-device=alsa/default:CARD=Audio'
                ;;
            esac

            if grep -q 'enabled' /sys/class/drm/card0-eDP-1/enabled && grep -q 'enabled' /sys/class/drm/{card0-DP-1,card0-DP-2,card0-HDMI-A-1}/enabled; then
                swaymsg -q output eDP-1 disable
                swaymsg -q -- exec \'/usr/bin/mpv $audio --player-operation-mode=pseudo-gui ${movs} \; grep -q open /proc/acpi/button/lid/LID/state \&\& swaymsg output eDP-1 enable\'
            else
                swaymsg -q -- exec \'/usr/bin/mpv $audio --player-operation-mode=pseudo-gui ${movs}\'
            fi
        fi
    }

    [[ ${pdfs} ]] && swaymsg -q -- exec \'/usr/bin/zathura ${pdfs}\'

    [[ ${pics} ]] && {
        [[ ${#pics} -eq 1 ]] && swaymsg -q -- exec \'/usr/bin/imv-wayland ${pics%/*} -n "${pics}"\' ||\
        swaymsg -q -- exec \'/usr/bin/imv-wayland ${pics}\'
    }

    [[ ${vscode} ]] && swaymsg -q -- exec \'electron17 /usr/lib/code/out/cli.js /usr/lib/code/code.js --enable-features=UseOzonePlatform --ozone-platform=wayland ${vscode}\' \; [app_id=^code-oss$] focus

    [[ ${docs} ]] && _docs_opener ${docs}

    [[ ${webs} ]] && {
        swaymsg -q -- exec \'/usr/bin/firefox ${webs[@]/#/--new-tab }\' \; [app_id=^firefox$] focus
    }

    [[ ${arcs} ]] && {
        typeset -a extract_msg
        local arc destination exit_code
        for arc in "${arcs[@]}"; do
            destination=$(__extracter_wrapper "$arc" "${explicit_extract_location}")
            exit_code="$?"
            (( exit_code > 2 )) && ret=5
            extract_msg+=$(_enum_exit_code $exit_code "$arc" "$destination")
        done
        if [[ ${#arcs[@]} -eq 1 ]] && [[ -d $destination ]] && [[ ${ret:-0} == 0 ]]; then
            cd "$destination"
        fi
        local msg
        for msg in ${extract_msg[@]}; do
            print "$msg"
        done
    } < $TTY || [[ ${ret} ]] || swaymsg -q -- [title=^PopUp$] move scratchpad > /dev/null 2>&1

    return ${ret:-0}
}

st_helper() {
    if [[ "$BUFFER" ]]; then
        if [[ $ST_ALIAS ]] && [[ "${LBUFFER: -$ST_ALIAS_LENGTH}" == " $ST_ALIAS" ]] || [[ $LBUFFER == "$ST_ALIAS" ]]; then
            local file
            read file < $__subl_file_path
            LBUFFER="${LBUFFER[1,-$ST_ALIAS_LENGTH]}$file "
            return 0
        fi
        LBUFFER+=" "
        return 0
    fi
    LBUFFER+="${_ZSH_FILE_OPENER_CMD} "
    zle expand-or-complete
}
zle -N st_helper
bindkey -e " " st_helper

remove_completion_insert_slash() {
    if [[ ${BUFFER:0:2} == "${_ZSH_FILE_OPENER_CMD} " ]] && (( ${#BUFFER} == 2 )); then
        LBUFFER+="/"
        zle .kill-line
        zle expand-or-complete
        return 0
    fi
    LBUFFER+="/"
}
zle -N remove_completion_insert_slash
bindkey -e "/" remove_completion_insert_slash

remove_completion_insert_tilde() {
    if [[ -z "$BUFFER" ]]; then
        LBUFFER+="$_ZSH_FILE_OPENER_CMD ~/"
        zle expand-or-complete
    elif [[ ${BUFFER:0:2} == "${_ZSH_FILE_OPENER_CMD} " ]] && (( ${#BUFFER} == 2 )); then
        LBUFFER+="~/"
        zle expand-or-complete
    else
        LBUFFER+='~'
    fi
}
zle -N remove_completion_insert_tilde
bindkey -e "~" remove_completion_insert_tilde

remove_completion_insert_tilde_with_backtick() {
    if [[ -z "$BUFFER" ]]; then
        LBUFFER+="$_ZSH_FILE_OPENER_CMD ~/"
        zle expand-or-complete
    elif [[ ${BUFFER:0:2} == "${_ZSH_FILE_OPENER_CMD} " ]] && (( ${#BUFFER} == 2 )); then
        LBUFFER+="~/"
        zle expand-or-complete
    else
        LBUFFER+='`'
    fi
}
zle -N remove_completion_insert_tilde_with_backtick
bindkey -e '`' remove_completion_insert_tilde_with_backtick
