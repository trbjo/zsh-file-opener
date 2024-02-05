#!/usr/bin/env zsh


file_opener(){
    local oldpwd="$PWD"
    integer count
    { open --only-files $@ && swaymsg -q -- '[app_id="^popup$"] move scratchpad'
    } 2>&1 >&- > /dev/null | () {
        local file destination
        while read file; do
            [[ -d "${file}" ]] && cd "$file" && continue
            case "${file:e:l}" in
            (${~_FILE_OPENER_ARCHIVE_FORMATS//,/|})
                cd "$oldpwd"
                destination=$(extract.sh "$file" "${explicit_extract_location}")
                _enum_exit_code $? "$file" "$destination" && cd "$destination"
                ;;
            (*)
                count+=1
                print $file
                ;;
           esac
        done
    }
    return $count
}

alias u='file_opener'
alias -g U='| open'
zstyle ':completion:*:*:file_opener:*:*' file-patterns '^*.(${_FILE_OPENER_EXCLUDE_SUFFIXES//,/|})' '*(D):all-files'
zstyle ':completion:*:*:file_opener:*:*' ignore-line other

if [[ $SSH_TTY ]]; then
    export EDITOR='rmate -w'
    export VISUAL='rmate -w'
    export _FILE_OPENER_EXCLUDE_SUFFIXES+=",$_FILE_OPENER_MULTIMEDIA_FORMATS,$_FILE_OPENER_BOOK_FORMATS,$_FILE_OPENER_PICTURE_FORMATS"
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
fi

    # if [[ "$BUFFER" ]]; then
        # if [[ $ST_ALIAS ]] && [[ "${LBUFFER: -$ST_ALIAS_LENGTH}" == " $ST_ALIAS" ]] || [[ $LBUFFER == "$ST_ALIAS" ]]; then
            # local file
            # read file < $__subl_file_path
            # LBUFFER="${LBUFFER[1,-$ST_ALIAS_LENGTH]}$file "
            # return 0
        # fi
        #
        # return 0
    # fi


fzy_prompt='${_PROMPT}%F{green}${PROMPT_SUCCESS_ICON} %f'
function _autosuggest_execute_or_clear_screen_or_ls() {

    if [[ $BUFFER ]]; then
        if [[ $POSTDISPLAY ]]; then
            BUFFER+="${POSTDISPLAY}"
            unset POSTDISPLAY
        fi
        zle .accept-line
        return
    fi

    local garbage termpos
    print -n "\x1b[3J\x1b[6n"        # clear scrollback buffer and ask the terminal for position
    read -d\[ garbage </dev/tty      # discard the first part of the response
    read -s -d R termpos </dev/tty   # store the position in bash variable 'termpos'
    typeset -i col="${termpos%%;*}"
    (( col <= ${PROMPT_WS_SEP+1} +1)) && {
        z | fzy --keep-output --show-info --prompt="$(print -Pn ${(e)fzy_prompt})" | () {
            while read file; do
                [[ -d "${file/#\~/${HOME}}" ]] && cd "${file/#\~/${HOME}}"
            done
        }

        zle fzf-redraw-prompt
    } || {
        printf "\x1b[?25l\x1b[H\x1b[2J\x1b[?25h"
        zle .reset-prompt
    }
}
zle -N _autosuggest_execute_or_clear_screen_or_ls
bindkey -e '\e' _autosuggest_execute_or_clear_screen_or_ls

zmodload -i zsh/complist
bindkey -M menuselect '\e' .accept-line

st_helper() {
    [[ -n "$BUFFER" ]] && LBUFFER+=" " && return
    integer isdir=1
    while (( $isdir )); do
        isdir=0
        { { printf '\x1b[36m..\n'; ls -A --color=always --group-directories-first -1;  } | fzy --keep-output --show-info --prompt="$(print -Pn ${(e)fzy_prompt})" | sed 's/^[ \t]*//;s/[ \t]*$//' | open --only-files && swaymsg -q -- '[app_id="^popup$"] move scratchpad'
        } 2>&1 >&- > /dev/null | () {
            while read file; do
                [[ -d "${file}" ]] && cd "$file" && isdir=1
            done
        }
        zle fzf-redraw-prompt
    done
}
zle -N st_helper
bindkey -e " " st_helper


go-home-widget() {
    [[ -n "$BUFFER" ]] && LBUFFER+='`' && return
    cd "$HOME"
    zle fzf-redraw-prompt
}
zle -N go-home-widget
bindkey '`' go-home-widget


alias -g home='/home/tb'
fzy-downloads-widget() {
        ls --color=always -ct1 "$HOME/dl" | \
            fzy --keep-output -il 30 --prompt="$(print -Pn ${(e)fzy_prompt})" | \
            awk '{print "~/dl/" $0}' | \
            open --only-files
        zle fzf-redraw-prompt
}
zle -N fzy-downloads-widget
bindkey '^O' fzy-downloads-widget
