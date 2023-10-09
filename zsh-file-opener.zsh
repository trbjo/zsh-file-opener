#!/usr/bin/env zsh

[[ -n "${_ZSH_FILE_OPENER_CMD}" ]] || _ZSH_FILE_OPENER_CMD=f
alias ${_ZSH_FILE_OPENER_CMD}='file_opener'
alias -g "${(U)_ZSH_FILE_OPENER_CMD}${(U)_ZSH_FILE_OPENER_CMD}"=' |& file_opener'
_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES=${_ZSH_FILE_OPENER_EXCLUDE_SUFFIXES:-"pdb,exe,srt,part,ytdl,vtt,zwc,dll,otf,ttf,iso,img,mobi,vtt"}
_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS=${_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS:-"mkv,mp4,movs,mp3,avi,mpg,m4v,oga,m4a,m4b,opus,wmv,mov,wav"}
_ZSH_FILE_OPENER_BOOK_FORMATS=${_ZSH_FILE_OPENER_BOOK_FORMATS:-"pdf,epub,djvu"}
_ZSH_FILE_OPENER_PICTURE_FORMATS=${_ZSH_FILE_OPENER_PICTURE_FORMATS:-"jpeg,jpg,png,webp,svg,gif,bmp,tif,tiff,psd,heic"}
_ZSH_FILE_OPENER_LIBREOFFICE_FORMATS=${_ZSH_FILE_OPENER_LIBREOFFICE_FORMATS:-"doc,docx,odt,ppt,pptx,odp"}
_ZSH_FILE_OPENER_GNUMERIC_FORMATS=${_ZSH_FILE_OPENER_GNUMERIC_FORMATS:-"ods,xls,xlsx"}
_ZSH_FILE_OPENER_WEB_FORMATS=${_ZSH_FILE_OPENER_WEB_FORMATS:-"SNTHSN"}
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

    function __subl() {
        swaymsg -q -- '[app_id=^popup$] move scratchpad; [app_id=^sublime_text|^subl$] focus; exec /opt/sublime_text/sublime_text'
        cat -  | /opt/sublime_text/sublime_text --fwdargv0 /usr/bin/zsh -
    }
    alias -g SS=' |& __subl'

        _docs_opener() {
            swaymsg -q -- "\
            [app_id=^popup$] move scratchpad;\
            [app_id=^sublime_text|^subl$] focus;\
            exec /opt/sublime_text/sublime_text ${docs}
            "
        }
    else
        _docs_opener() {
            [[ -z $EDITOR ]] && print '$EDITOR not set' && return 1
            $EDITOR ${docs}
        }
    fi
fi




getNameOfTorrent() {
    # stringified=$(sed -e 's/%20/ /g' <<< $1)
    stringified="${1:gs/%20/ }"
    stringified="${stringified:gs/%26/\&}"
    local IFS='='
    while read -r prot hash name tracker
    do
        printf %s "${name: 0:-3}"
    done <<< "$stringified"
}


transmission_function() {
    local message icon action file
    if [[ ${#@} -gt 1 ]]; then
        message="${#@} torrents"
    elif [[ ${1: 0:6} == "magnet" ]]; then
        message=$(getNameOfTorrent $1)
    else
        message="Torrent"
    fi
    message="$message added"
    icon="com.github.davidmhewitt.torrential"
    action='swaymsg -q -- [title=\"^Transmission Web Interface \"] focus || xdg-open http://127.0.0.1:9091/transmission/web/'
    notify-send.sh "Transmission" "$message" --icon=$icon --default-action=$action

    # if /usr/bin/ssh tb@51.154.197.131 "transmission-remote -a '"${@}"'"; then
    # /usr/bin/ssh tb@51.154.197.131 "transmission-remote -a '"${@}"'" && message $added

    systemctl --user enable --now transmission.service

    for file in "${@}"; do
        transmission-remote -a "${file}"
    done
}


file_opener() {
    local url='^about:|^((ftp://)(magnet:)||(https?://))?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*)?$'

    typeset -aU arcs movs pdfs pics webs docs dirs batstatus vscode disabled array libre gnumeric
    local ret arg

    if [[ -t 0 ]] ||  [[ ! $- == *i* ]]; then
        [[ -z "$@" ]] && cd > /dev/null 2>&1 && return 0
        for arg in "$@"; do
            case "${arg}" in
                (-c|--create) local _create=true ;;
                (-m|--files-with-matches) local __rg=true ;;
                (-f|--force)  local _ZSH_FILE_OPENER_EXCLUDE_SUFFIXES="" ;;
                (-t|--text)   local _OPEN_IN_TEXT_EDITOR=true ;;
                (-w|--web)
                    local _zsh_file_opener_web_formats
                    if [[ ! -z "$_ZSH_FILE_OPENER_WEB_FORMATS" ]]; then
                        _zsh_file_opener_web_formats="${_ZSH_FILE_OPENER_WEB_FORMATS},html,mhtml"
                    else
                        _zsh_file_opener_web_formats="html,mhtml"
                    fi
                 ;;
                (*) array+=("$arg") ;;
            esac
        done
        [[ $__rg ]] && [[ -n "${array}" ]] && {
            /usr/bin/rg --files-with-matches $array | file_opener
            return
        }

    else
        # turn stdin into args -- useful in combination with grep --files-with-matches
        while read -r arg ; do
            array+=("$arg")
        done
    fi
    local webz=${_ZSH_FILE_OPENER_WEB_FORMATS:-$_zsh_file_opener_web_formats}

    for file in "${array[@]}"; do
        if [[ $file =~ ${~url} ]] && [[ ! -e "$file" ]] && [[ ! $- == *i* ]]; then
            webs+=("${file}")
            continue
        fi

        if [[ ! -r "${file}" ]]; then
            if [[ ! -z $_create ]]; then
                mkdir -p "${${file:a}%/*}"
            # it is only an error if we cannot read the containing dir, OR if we cannot read the file
            elif [[ -e "${file%/}" ]] || [[ ! -r "${${file:a}%/*}" ]]; then
                local -a dir_parts=( ${(s[/])${file:a}} )
                local assembly="/"
                for num in {1..${#dir_parts[@]}}; do
                    assembly+="${dir_parts[$num]}"
                    if [[ ! -r "$assembly" ]]; then
                        if [[ -e "$assembly" ]]; then
                            print "Permission denied: $(_colorizer $assembly)"
                        else
                            print "Directory does not exist: $(_colorizer $assembly)"
                        fi
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
                    print $file
                   [[ $- == *i* ]] && disabled+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_ARCHIVE_FORMATS//,/|})
                    arcs+=(${file:a})
                    [[ "${#@}" -eq 2 ]] && { local explicit_extract_location="$2"; break } ;;
                (${~_ZSH_FILE_OPENER_MULTIMEDIA_FORMATS//,/|})
                    movs+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_BOOK_FORMATS//,/|})
                    swaymsg -q "[app_id=\"^org.pwmt.zathura$\" title=\"^${(q)file##*/}\ \[\"] focus" || pdfs+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_PICTURE_FORMATS//,/|})
                    pics+=("${file:A:q}") ;;
                (${~webz//,/|})
                    webs+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_LIBREOFFICE_FORMATS//,/|})
                    libre+=("${file:A:q}") ;;
                (${~_ZSH_FILE_OPENER_GNUMERIC_FORMATS//,/|})
                    gnumeric+=("${file:A:q}") ;;
                (*)
                    [[ ! $- == *i* ]] && [[ ! -e "${file:A:q}" ]] && webs+=("$file") && break
                    [[ "${#@}" -eq 2 ]] && [ $2 -gt 0 2>/dev/null ] && docs+=("${file:A:q}":$2) && break
                    docs+=("${file:A:q}") ;;
            esac
        fi
    done

    [[ ${dirs} ]] && {
        if [[ ${#dirs} -eq 1 ]]; then
            # print lol
            if [[ $- == *i* ]]; then # as func
                cd "$dirs" && ret=${ret:-0}
            else # as file
                footclient -D $dirs
            fi
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

    [[ ${movs} ]] && () {
        if pgrep -x mpv > /dev/null 2>&1; then
            [[ ! -S /tmp/mpvsocket ]] && print "mpvsocket not found" && ret=1 && return
            for movie in ${movs[@]}; do
                print "loadfile ${(qq)movie} append" | socat - /tmp/mpvsocket
                notify-send.sh "${movie##*/}" "Playing next…" --default-action="swaymsg -q '[app_id=^mpv$] focus'"
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

    [[ ${pdfs} ]] && {
        local pdf_str=""
        pdfs=("${(@on)pdfs}")
        for pdf in $pdfs; do
            pdf_str+="/usr/bin/zathura $pdf &!; "
        done
        swaymsg -q -- exec \'$pdf_str\'
    }

    [[ ${pics} ]] && {
        pics=("${(@on)pics}")
        [[ ${#pics} -eq 1 ]] && swaymsg -q -- exec \'/usr/bin/imv-wayland ${pics%/*} -n "${pics}"\' ||\
        swaymsg -q -- exec \'/usr/bin/imv-wayland ${pics}\'
        # swaymsg -q -- exec \'eog $pics\'
    }

    [[ ${libre} ]] && {
        swaymsg -q -- "exec /usr/bin/libreoffice --norestore ${libre[@]}; [app_id=^libreoffice] focus"
    }
    [[ ${gnumeric} ]] && {
        swaymsg -q -- "exec /usr/bin/gnumeric ${gnumeric[@]}; [app_id=^gnumeric] focus"
    }


    [[ ${webs} ]] && {
        local web
        typeset -a torrents

        for web in $webs; do
            if [[ $web == magnet* ]]; then
                torrents+=("${web}")
            else
                firefox ${webs[@]}
            fi
        done
        [[ ${torrents} ]] && transmission_function ${torrents}
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
    } < $TTY

    [[ ${docs} ]] && _docs_opener ${docs}

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

[[ ! $- == *i* ]] && file_opener "$@"
