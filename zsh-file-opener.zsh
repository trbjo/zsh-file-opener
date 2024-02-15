fzy-redraw-prompt() {
    local precmd
    for precmd in $precmd_functions; do
        $precmd
    done
}
zle -N fzy-redraw-prompt

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

function easy-clear-screen() {
    if [[ $BUFFER ]]; then
        [[ $POSTDISPLAY ]] && unset POSTDISPLAY
        LBUFFER+='`'
        return
    fi

    printf "\x1b[3J\x1b[H\x1b[2J"
    zle fzy-redraw-prompt
    zle reset-prompt
    return
}

zle -N easy-clear-screen
# bindkey '`' easy-clear-screen


function __incremental_git() {
    print -n "\r\x1b[K\x1b[J"
    zle -I
    $@
    printf '\033[0;36m' # Set color to cyan
    printf '%0.1s' '─'{1..${COLUMNS:-$(tput cols)}}
    printf '\033[0m'
    printf '\n'
    zle -R
}

function fzy-gitstatus() {
    local fd
    typeset -A FDS=(1 stdout 3 fd3 4 fd4 5 fd5 6 fd6 8 fd8 9 fd9)
    for fd in ${(v)FDS}; typeset $fd=$(mktemp)

    typeset -a files
    typeset -a untracked_files

    integer keep_at_it=1
    while (( keep_at_it )); do

        for key val in ${(kv)FDS}; do eval "exec {${(U)val}}>${(P)val}" ;done

        # $val             # -> "stdout"
        # ${(P)${(U)val}}  # -> "45"
        # ${(P)${val}}     # -> "/tmp/tmp.dFrjurrPw2"
        # ${${(U)val}}     # -> "STDOUT"

        git -c color.status=always status --short --untracked-files=all $PWD | \
        fzy --columns=1 \
            --keep-output \
            --prompt="$(print -Pn ${(e)PROMPT})" \
            1>&${(P)${(U)${FDS[1]}}} \
            3>&${(P)${(U)${FDS[3]}}} \
            4>&${(P)${(U)${FDS[4]}}} \
            5>&${(P)${(U)${FDS[5]}}} \
            6>&${(P)${(U)${FDS[6]}}} \
            8>&${(P)${(U)${FDS[8]}}} \
            9>&${(P)${(U)${FDS[9]}}} \
            || keep_at_it=0


        for key val in ${(kv)FDS}; do

            eval "exec {${(U)val}}>&-"
            eval "exec {${(U)val}}<${(P)${val}}"
            files=()
            untracked_files=()
            while IFS= read -r line; do

                file="${line[4,-1]}"
                [[ -f "$file" ]] || continue

                case $key in
                    1) # open
                        files+=(${file})
                        ;;
                    9) # add -p
                        [[ ${line[2]} == ' ' ]] && continue
                        [[ ${line[1,2]} == '??' ]] && continue
                        files+=(${file})
                        ;;
                    8) # reset -p
                        [[ ${line[1]} == ' ' ]] && continue
                        [[ ${line[1,2]} == '??' ]] && continue
                        files+=(${file})
                        ;;
                    3) # add file
                        git add "$file"
                        ;;
                    4) # checkout/remove file
                        if [[ "${line[1,2]}" == '??' ]]; then # remove
                            rm "$file"
                        elif [[ "${line[1,1]}" != ' ' ]]; then # unstage file
                            git restore --staged "${file}"
                        else
                            git checkout "${file}" # revert to index
                        fi
                        ;;
                    5) # view diff
                        [[ "${line[1,2]}" == '??' ]] && untracked_files+=($file) || files+=($file)
                        ;;
                    6) # wl-copy
                        keep_at_it=0
                        files+=(${file})
                        ;;
                esac
            done <&${(P)${(U)val}}
            eval "exec {${(U)val}}>&-"

            (( key == 1 )) && (( #files )) && open --attach ${files}

            (( key == 9 )) && (( #files )) && __incremental_git git add -p $files </dev/tty
            (( key == 8 )) && (( #files )) && __incremental_git git reset -p $files </dev/tty

            (( key == 5 )) && (( #files )) && git diff -- $files
            # (( key == 5 )) && (( #files )) && git diff HEAD -- $files
            (( key == 5 )) && (( #untracked_files )) && git diff --no-index /dev/null -- $untracked_files

            (( key == 6 )) && (( #files )) && wl-copy -n <<< ${files}
        done
    done
    zle fzy-redraw-prompt
    zle .reset-prompt

    for fd in ${(v)FDS}; rm ${(P)fd}
}
zle -N fzy-gitstatus
bindkey '^P' fzy-gitstatus

function glo(){
    typeset -A FDS=(1 stdout 6 fd6)
    for fd in ${(v)FDS}; typeset $fd=$(mktemp)

    integer keep_at_it=1
    data=$(git log \
            --date=format-local:"%Y-%m-%d %H:%M" \
            --pretty=format:"%C(red)%h %C(green)%cd%C(reset) %C(cyan)●%C(reset) %C(yellow)%an%C(reset) %C(cyan)●%C(reset) %s" \
            --abbrev-commit \
            --color=always
           )
        local hash=$(sed 's/\x1b\[[0-9;]*m//g; s/ .*//' <<< $(read first <<< $data; print -n $first))
        integer hashlen=${#hash}
    while (( keep_at_it )); do
        wl-copy -n <<< '●'
        for key val in ${(kv)FDS}; do eval "exec {${(U)val}}>${(P)val}" ;done

        # $val             # -> "stdout"
        # ${(P)${(U)val}}  # -> "45"
        # ${(P)${val}}     # -> "/tmp/tmp.dFrjurrPw2"
        # ${${(U)val}}     # -> "STDOUT"

        fzy --columns=1 \
            --keep-output \
            --hide-first=$((hashlen+1)) \
            --prompt="$(print -Pn ${(e)PROMPT})" <<< $data \
            1>&${(P)${(U)${FDS[1]}}} \
            6>&${(P)${(U)${FDS[6]}}} \
            || keep_at_it=0

        for key val in ${(kv)FDS}; do

            eval "exec {${(U)val}}>&-"
            eval "exec {${(U)val}}<${(P)${val}}"
            commits=()
            while IFS= read -r line; do

                commit="${line[1,$hashlen]}"

                case $key in
                    1) # show
                        commits+=(${commit})
                        ;;

                    6) # wl-copy
                        keep_at_it=0
                        commits+=(${commit})
                        ;;

                esac
            done <&${(P)${(U)val}}
            eval "exec {${(U)val}}>&-"

            (( key == 1 )) && (( #commits )) && git show $commits
            (( key == 6 )) && (( #commits )) && wl-copy -n <<< ${commits}

        done
    done

    for fd in ${(v)FDS}; rm ${(P)fd}
    return 0
}



fzy-widget() {
    local loop=$1
    shift

    local fd
    typeset -A FDS=(1 stdout 6 fd6 9 fd9)
    for fd in ${(v)FDS}; typeset $fd=$(mktemp)

    integer keep_at_it=1
    while (( keep_at_it )); do

        for key val in ${(kv)FDS}; do eval "exec {${(U)val}}>${(P)val}" ;done

        # $val             # -> "stdout"
        # ${(P)${(U)val}}  # -> "45"
        # ${(P)${val}}     # -> "/tmp/tmp.dFrjurrPw2"
        # ${${(U)val}}     # -> "STDOUT"

        eval $* | \
        fzy \
            --keep-output \
            --prompt="$(print -Pn ${(e)PROMPT})" \
            1>&${(P)${(U)${FDS[1]}}} \
            6>&${(P)${(U)${FDS[6]}}} \
            9>&${(P)${(U)${FDS[9]}}} \
            || keep_at_it=0

        for key val in ${(kv)FDS}; do

            eval "exec {${(U)val}}>&-"
            eval "exec {${(U)val}}<${(P)${val}}"
            files=()
            untracked_files=()
            while IFS= read -r line; do

                case $key in
                    1) # open
                        IFS=' ' read trimmed <<<"$line"
                        files+=(${trimmed})
                        ;;
                    9) # add to buffer
                        files+=(${line})
                        keep_at_it=0
                        ;;
                    6) # wl-copy
                        keep_at_it=0
                        files+=(${line})
                        ;;
                esac
            done <&${(P)${(U)val}}
            eval "exec {${(U)val}}>&-"

            (( key == 1 )) && (( #files )) && {
                {
                open \
                  --only-files $files && \
                swaymsg \
                  -q -- '[app_id="^popup$"] move scratchpad'
                } 2>&1 | \
                while IFS= read -r file; do
                    if [[ -d "${file}" ]] ;then
                        cd "$file"
                        keep_at_it=$loop
                    fi
                done
            }

            (( key == 9 )) && (( #files )) && {
                BUFFER+=$files
            }

            (( key == 6 )) && (( #files )) && wl-copy -n <<< ${files}
        done
        zle reset-prompt
    done
    zle fzy-redraw-prompt
    zle reset-prompt
    for fd in ${(v)FDS}; rm ${(P)fd}

}
zle -N fzy-widget

ls-widget() {
    [[ -n "$BUFFER" ]] && LBUFFER+=" " && return
    zle fzy-widget 1 "printf '\x1b[36m..\n'; ls -A --color=always --group-directories-first -1"
}
zle -N ls-widget
bindkey -e " " ls-widget

fzy-history-widget() {
    attempt=$(fc -rlit "%d-%m %H.%M" 0 | \
          sed -r "s/^(.{19})./ \1│/" | \
          fzy \
              --keep-output \
              --lines=15 \
              --columns=1 \
              --hide-first=7 \
              --skip-search=14 \
              --query="$BUFFER" \
              --prompt="$(print -Pn ${(e)PROMPT})")
    [[ ! -z $attempt ]] && BUFFER="${history[${$(xargs <<< $attempt)%% *}]:-$attempt}"
    CURSOR=$#BUFFER
    zle fzy-redraw-prompt
    zle reset-prompt

}
zle -N fzy-history-widget
bindkey '^R' fzy-history-widget



_zsh_z_widget() {
    if [[ $BUFFER ]]; then
        if [[ $POSTDISPLAY ]]; then
            BUFFER+="${POSTDISPLAY}"
            unset POSTDISPLAY
        fi
        zle .accept-line
        return
    fi
    zle fzy-widget 0 "command z"
}
zle -N _zsh_z_widget
bindkey -e '\e' _zsh_z_widget

function _zshz(){
    input="${${*// /}/#$HOME/~}"
    file=${$(command z | fzy --query="$input" --pick-only --prompt="$(print -Pn ${(e)PROMPT})")/#\~/${HOME}}
    [[ -d ${file} ]] && cd ${file} && print -S "h $input"
}
alias h='noglob _zshz'

zmodload -i zsh/complist
bindkey -M menuselect '\e' .accept-line


fzy-downloads-widget() {
        ls --color=always -ct1 "$HOME/dl" | \
            fzy --keep-output -il 30 --prompt="$(print -Pn ${(e)PROMPT})" | \
            awk '{print "~/dl/" $0}' | \
            open --only-files
        zle fzy-redraw-prompt
        zle reset-prompt
}
zle -N fzy-downloads-widget
# bindkey '^O' fzy-downloads-widget


autoload edit-command-line; zle -N edit-command-line
bindkey -e '^O' edit-command-line


__register_con_id() {
    [[ -z $SWAYSOCK ]] || export FILE_OPENER_CALLBACK="$(swaymsg -t get_tree | jq '.. | select(.type?) | select(.focused==true) | .id')"
    add-zsh-hook -d preexec __register_con_id
    unfunction __register_con_id
}

autoload -Uz add-zsh-hook
add-zsh-hook preexec __register_con_id
