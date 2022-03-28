alias ${_ZSH_FILE_OPENER_CMD:-f}='_file_opener'
_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES=${_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES:-"otf,ttf,iso"}
_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS=${_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS:-"mkv,mp4,movs,mp3,avi,mpg,m4v,oga,m4a,m4b,opus,wmv,mov"}
_ZSH_FILE_OPENER_BOOK_FORMATS=${_ZSH_FILE_OPENER_BOOK_FORMATS:-"pdf,epub,djvu"}
_ZSH_FILE_OPENER_PICTURE_FORMATS=${_ZSH_FILE_OPENER_PICTURE_FORMATS:-"jpeg,jpg,png,webp,svg,gif,bmp,tif,tiff,psd"}
_ZSH_FILE_OPENER_WEB_FORMATS=${_ZSH_FILE_OPENER_WEB_FORMATS:-"html,mhtml"}
_ZSH_FILE_OPENER_ARCHIVE_FORMATS=${_ZSH_FILE_OPENER_ARCHIVE_FORMATS:-"gz,tgz,bz2,tbz,tbz2,xz,txz,zma,tlz,zst,tzst,tar,lz,gz,bz2,xz,lzma,z,zip,war,jar,sublime-package,ipsw,xpi,apk,aar,whl,rar,rpm,7z,deb,zs"}
zstyle ':completion:*:*:_file_opener:*:*' file-patterns '^*.(${_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES//,/|})' '*(D):all-files'
zstyle ':completion:*:*:_file_opener:*:*' ignore-line other

if [[ $SSH_TTY ]]; then
    export EDITOR='rmate -w'
    export VISUAL='rmate -w'
    _ZSH_FILE_OPENER_EXCLUDE_SUFFIXES+=",$_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS,$_ZSH_FILE_OPENER_BOOK_FORMATS,$_ZSH_FILE_OPENER_PICTURE_FORMATS"
    alias -g SS=' |& rmate -'

    if [[ ! -f "$HOME/.local/bin/rmate" ]]; then
        curl https://raw.githubusercontent.com/aurora/rmate/master/rmate > "$HOME/.local/bin/rmate"
        chmod +x "$HOME/.local/bin/rmate"
    fi
    if [[ $commands[doas] ]]; then
        root_cmd() { doas $(where rmate) "$@" }
    elif [[ $commands[sudo] ]]; then
        root_cmd() { sudo $(where rmate) "$@" }
    else
        root_cmd() { echo "No root escalation command like sudo or doas found. Try su instead." }
    fi

    _docs_opener() {
        for doc in ${docs[@]}; do
            if [[ -w "${${doc:a}%/*}" ]]; then
                rmate "$doc"
            else
                root_cmd "$doc"
            fi
        done
    }
else
    alias -g SS=' |& subl -'
    _docs_opener() { swaymsg -q -- exec \'/opt/sublime_text/sublime_text ${docs}\' \; [app_id=^sublime_text$] focus }
fi

_file_opener() {
    typeset -aU arcs movs pdfs pics webs docs dirs batstatus vscode disabled array
    local ret
    [[ -z "$@" ]] && cd > /dev/null 2>&1 && return 0
    for arg in "$@"; do
        case "${arg}" in
            (-c|--create) local _create=true ;;
            (-f|--force) local _ZSH_FILE_OPENER_EXCLUDE_SUFFIXES="";;
            (-t|--text) local _OPEN_IN_TEXT_EDITOR=true ;;
            (*) array+=("$arg") ;;
        esac
    done
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
            docs+=("${file:a:q}")
            continue
        elif [[ -d ${file} ]]; then
            dirs+=("$file")
            continue
        else
            case "${file:e:l}" in
                (${~_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES//,/|})
                    disabled+=("${file:a:q}") ;;
                (${~_ZSH_FILE_OPENER_ARCHIVE_FORMATS//,/|})
                    arcs+=(${file:a})
                    [[ "${#@}" -eq 2 ]] && { local explicit_extract_location="$2"; break } ;;
                (${~_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS//,/|})
                    swaymsg -q '[app_id=mpv] focus' || movs+=("${file:a:q}") ;;
                (${~_ZSH_FILE_OPENER_BOOK_FORMATS//,/|})
                    swaymsg -q "[app_id=\"^org.pwmt.zathura$\" title=\"^${(q)file##*/}\ \[\"] focus" || pdfs+=("${file:a:q}") ;;
                (${~_ZSH_FILE_OPENER_PICTURE_FORMATS//,/|})
                    pics+=("${file:a:q}") ;;
                (${~_ZSH_FILE_OPENER_WEB_FORMATS//,/|})
                    webs+=("${file:a:q}") ;;
                (ipynb)
                    vscode+=("${file:a:q}") ;;
                (*)
                    [[ "${#@}" -eq 2 ]] && [ $2 -gt 0 2>/dev/null ] && docs+=("${file:a:q}":$2) && break
                    docs+=("${file:a:q}") ;;
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
    } < $TTY || [[ ${ret} ]] || swaymsg -q -- [app_id=^PopUp$] move scratchpad > /dev/null 2>&1

    [[ ${movs} ]] && {
        if [[ -e /sys/class/power_supply/AC*/online ]]; then
            grep -q 'enabled' /sys/class/drm/{card0-DP-1,card0-DP-2,card0-HDMI-A-1}/enabled\
            && read batstatus < /sys/class/power_supply/AC*/online && [[ 0 == $batstatus ]]\
            && swaymsg -q output eDP-1 dpms off
        fi
        swaymsg -q -- exec \'/usr/bin/mpv --player-operation-mode=pseudo-gui ${movs} \; swaymsg output eDP-1 dpms on\'
    }
    [[ ${pdfs} ]] && swaymsg -q -- exec \'/usr/bin/zathura ${pdfs}\'

    [[ ${pics} ]] && {
        [[ ${#pics} -eq 1 ]] && swaymsg -q -- exec \'/usr/bin/imv-wayland ${pics%/*} -n "${pics}"\' ||\
        swaymsg -q -- exec \'/usr/bin/imv-wayland ${pics}\'
    }

    [[ ${vscode} ]] && swaymsg -q -- exec \'electron13 /usr/lib/code/out/cli.js /usr/lib/code/code.js --enable-features=UseOzonePlatform --ozone-platform=wayland ${vscode}\' \; [app_id=^code-oss$] focus

    [[ ${docs} ]] && _docs_opener ${docs}

    [[ ${webs} ]] && {
        swaymsg -q -- exec \'/usr/bin/firefox ${webs[@]/#/--new-tab }\' \; [app_id=^firefox$] focus
    } || [[ ${ret} ]]

    return ${ret:-0}
}

# we bind slash, space tilde and backtick to help with this function
file_opener_helper() {
    if [[ "$BUFFER" ]]; then
        LBUFFER+=" "
        return 0
    fi
    LBUFFER+="${_ZSH_FILE_OPENER_CMD} "
    zle expand-or-complete
    typeset -g _did_complete
}
zle -N file_opener_helper
bindkey -e " " file_opener_helper


remove_completion_insert_slash() {
    if [[ -z "$BUFFER" ]]; then
        LBUFFER+="$_ZSH_FILE_OPENER_CMD /"
        zle expand-or-complete
    elif [[ ${BUFFER:0:2} == "${_ZSH_FILE_OPENER_CMD} " ]] && (( ${+_did_complete} )); then
        unset _did_complete
        zle .undo
        LBUFFER+="$_ZSH_FILE_OPENER_CMD /"
        zle expand-or-complete
    else
        LBUFFER+='/'
    fi
}
zle -N remove_completion_insert_slash
bindkey -e "/" remove_completion_insert_slash

remove_completion_insert_tilde() {
    if [[ -z "$BUFFER" ]]; then
        LBUFFER+="$_ZSH_FILE_OPENER_CMD ~/"
        zle expand-or-complete
    elif [[ ${BUFFER:0:2} == "${_ZSH_FILE_OPENER_CMD} " ]] && (( ${+_did_complete} )); then
        unset _did_complete
        zle .undo
        LBUFFER+="$_ZSH_FILE_OPENER_CMD ~/"
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
    elif [[ ${BUFFER:0:2} == "${_ZSH_FILE_OPENER_CMD} " ]] && (( ${+_did_complete} )); then
        unset _did_complete
        zle .undo
        LBUFFER+="$_ZSH_FILE_OPENER_CMD ~/"
        zle expand-or-complete
    else
        LBUFFER+='`'
    fi
}
zle -N remove_completion_insert_tilde_with_backtick
bindkey -e '`' remove_completion_insert_tilde_with_backtick
