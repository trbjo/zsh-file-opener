# Paste the selected command(s) from history into the command line
fzf-history-widget() {
    newline=$'\n'
    _prompt=$(print -Pn ${${(e)PROMPT}##*${newline}})

    format_start=$'\033[4m'
    format_end=$'\033[0m \033[37m│\033[0m'
    numbers=($(fc -rli 1 | \
    sed -r "s/^ {0,4}([0-9]{1,5}).(.{17})./\1$format_start\2$format_end/" | \
    fzf \
     --delimiter=' ' \
     --with-nth=2.. \
     --no-sort \
     --prompt="$_prompt" \
     --no-extended \
     --bind 'enter:execute(echo {+1})+abort' \
     --no-hscroll \
     --query="${LBUFFER}" \
     --preview-window='bottom,30%' \
     --preview "xargs -0 <<< {5..}"))
    LBUFFER+="${history[${numbers[1]}]}"
    for number in ${numbers[@]:1}; LBUFFER+=$'\n'${history[$number]}
    zle reset-prompt
}
zle -N fzf-history-widget
bindkey '^R' fzf-history-widget


export FZF_DEFAULT_OPTS="${FZF_DEFAULT_OPTS} \
--bind \"alt-t:page-down,\
alt-c:page-up,\
ctrl-e:replace-query,\
ctrl-b:toggle-all,\
change:top,\
ctrl-r:toggle-sort,\
ctrl-q:unix-line-discard\" \
--preview-window=right:50%:sharp:wrap \
--preview 'if [[ -d {} ]]
    then
        fancyls --color=always --hyperlink=always {}
    elif [[ {} =~ \"\.(jpeg|JPEG|jpg|JPG|png|webp|WEBP|PNG|gif|GIF|bmp|BMP|tif|TIF|tiff|TIFF)$\" ]]
    then
        identify -ping -format \"%f\\n%m\\n%w x %h pixels\\n%b\\n\\n%l\\n%c\\n\" {}
    elif [[ {} =~ \"\.(svg|SVG)$\" ]]
    then tiv -h \$FZF_PREVIEW_LINES -w \$FZF_PREVIEW_COLUMNS {}
    elif [[ {} =~ \"\.(pdf|PDF)$\" ]]
    then pdfinfo {}
    elif [[ {} =~ \"\.(zip|ZIP|sublime-package)$\" ]]
    then zip -sf {}
    elif [[ {} =~ \"(json|JSON)$\" ]]
    then jq --indent 4 --color-output < {}
else bat \
    --style=header,numbers \
    --terminal-width=\$((\$FZF_PREVIEW_COLUMNS - 6)) \
    --force-colorization \
    --italic-text=always \
    --line-range :70 {} 2>/dev/null; fi'"

alias fif='noglob _fif'
_fif() {
    [[ -z "$@" ]] && print "Need a string to search for!" && return 1
    rg \
        --files-with-matches \
        --no-messages \
        "$@" | \
    fzf \
        --prompt="$(print -Pn "${PROMPT_PWD:-$PWD} \e[3m$myQuery\e[0m") " \
        --preview "rg $RIPGREP_OPTS --pretty --context 10 '$@' {}"
}


# Ensure precmds are run after cd
fzf-redraw-prompt() {
    local precmd
    for precmd in $precmd_functions; do
        $precmd
    done
    zle reset-prompt
}
zle -N fzf-redraw-prompt

alias myfzf="fzf \
        --bind 'ctrl-h:change-preview-window(right,75%|hidden|right,50%)' \
        --preview-window=right,50%,border-left"
alias -g F='| myfzf'



if type pass > /dev/null 2>&1; then
fzf-password() {
    /usr/bin/fd . --extension gpg --base-directory $HOME/.password-store |\
     sed -e 's/.gpg$//' |\
     sort |\
     fzf --no-multi --preview-window=hidden --bind 'alt-w:abort+execute-silent@touch /tmp/clipman_ignore ; wl-copy -n -- $(pass {})@,enter:execute-silent@ if [[ $PopUp ]]; then swaymsg "[app_id=^PopUp$] scratchpad show"; fi; touch /tmp/clipman_ignore; wl-copy -n -- $(pass {})@+abort'
}
zle -N fzf-password
fi

alias glo="eval 'myp=\$(print -Pn \${_PROMPT})'
    git log \
        --date=format-local:'%Y-%m-%d %H:%M' \
        --pretty=format:'%C(red)%h %C(green)%cd%C(reset) %C(cyan)●%C(reset) %C(yellow)%an%C(reset) %C(cyan)●%C(reset) %s' \
        --abbrev-commit \
        --color=always | \
    fzf \
        --header=\"\$myp\" \
        --header-first \
        --delimiter=' ' \
        --no-sort \
        --no-extended \
        --with-nth=2.. \
        --bind 'enter:become(print -l -- {+1})' \
        --bind 'alt-w:execute-silent(wl-copy -n -- {+1})+abort' \
        --bind 'ctrl-h:change-preview-window(down,75%|down,99%|hidden|down,50%)' \
        --bind 'ctrl-b:put( ● )' \
        --preview='
        typeset -a args=(--hyperlinks --width=\$(( \$FZF_PREVIEW_COLUMNS - 2)));
        [[ \$FZF_PREVIEW_COLUMNS -lt 160 ]] || args+=--side-by-side
        git show --color=always {1} | delta \$args' \
        --preview-window=bottom,50%,border-top"

load='_gitstatus=$(git -c color.status=always status --short --untracked-files=all $PWD)
    (( ${#_gitstatus} > 1 )) && {
       rg "^\x1b\[32m.\x1b\[m" <<< $_gitstatus
    rg -v "^\x1b\[32m.\x1b\[m" <<< $_gitstatus &!
    }'

resetterm=$'\033[2J\033[3J\033[H'
cyan=$'\e[1;36;m'
magenta=$'\e[0;35;m'
white=$'\e[0;37;m'
reset=$'\e[0;m'
quote='\\\"'

alias gs="\
    eval 'myp=\$(print -Pn \${_PROMPT})'
    $load | fzf \
        --header=\"\$myp\" \
        --header-first \
        --delimiter='' \
        --exit-0 \
        --nth='4..' \
        --no-sort \
        --no-extended \
        --bind 'enter:become(print -l {+4..} | sed -e 's/^${quote}//' -e 's/${quote}$//')' \
        --bind 'ctrl-p:execute-silent(open {+4..})+become(print -l {+4..} | sed -e 's/^${quote}//' -e 's/${quote}$//')' \
        --bind 'ctrl-a:execute-silent(git add {+4..})+reload($load)' \
        --bind 'ctrl-c:execute-silent(git checkout {+4..})+reload($load)' \
        --bind 'ctrl-/:execute-silent(rm {+4..})+reload($load)' \
        --bind 'ctrl-r:execute-silent(git restore --staged {+4..})+reload($load)' \
        --bind 'ctrl-n:execute(git add -p {+4..}; printf \"$resetterm\")+reload($load)' \
        --bind 'ctrl-h:change-preview-window(down,75%|down,99%|hidden|down,50%)' \
        --preview '
        typeset -a args=(--hyperlinks --width=\$(( \$FZF_PREVIEW_COLUMNS - 2)));
        [[ \$FZF_PREVIEW_COLUMNS -lt 160 ]] || args+=--side-by-side
        if [[ {} == \"?*\" ]]; then
                          git diff --no-index /dev/null {4..} | delta \$args;
                      else
                          git diff HEAD -- {4..} | delta \$args;
                      fi;' \
        --preview-window=bottom,50%,border-top"

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

function easy-clear-screen() {
    if [[ $BUFFER ]]; then
        [[ $POSTDISPLAY ]] && unset POSTDISPLAY
        LBUFFER+='`'
        return
    fi

    printf "\x1b[3J\x1b[H\x1b[2J"
    zle fzf-redraw-prompt
    return
}

zle -N easy-clear-screen
bindkey '`' easy-clear-screen


fzy_prompt='${_PROMPT}%F{green}${PROMPT_SUCCESS_ICON} %f'
fzy-widget() {
    local loop=$1
    shift
    integer isdir=1
    local mytemp=$(mktemp)
    exec {FD}>$mytemp
    while (( $isdir )); do
        isdir=0;
        eval $* | \
        fzy --keep-output --show-info --prompt="$(print -Pn ${(e)fzy_prompt})" \
        3> >(wl-copy) \
        4>&${FD} | \
        sed 's/^[ \t]*//;s/[ \t]*$//' | \
        {
            open --only-files && \
            swaymsg -q -- '[app_id="^popup$"] move scratchpad'
        } 2>&1 | \
        while IFS= read -r file; do
            if [[ -d "${file}" ]] ;then
                cd "$file"
                isdir=$loop
            fi
        done
        (( $isdir )) && continue

        exec {FD}>&-
        exec {FD}<$mytemp
        while read -r line; do
            isdir=0
            BUFFER+=" $line"
        done <&${FD}
        exec {FD}>&-
    done
    rm $mytemp
    zle fzf-redraw-prompt
}
zle -N fzy-widget

ls-widget() {
    [[ -n "$BUFFER" ]] && LBUFFER+=" " && return
    zle fzy-widget 1 "printf '\x1b[36m..\n'; ls -A --color=always --group-directories-first -1"
}
zle -N ls-widget
bindkey -e " " ls-widget

fd-widget() {
    zle fzy-widget 1 "fd --color=always --hidden --exclude node_modules"
}
zle -N fd-widget


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
    _fzy_prompt="$(print -Pn ${(e)fzy_prompt})"
    file="${$(command z | fzy --query="${*// /}" --keep-output --show-scores --show-info --prompt=$_fzy_prompt)/#\~/${HOME}}"
    [[ -d "${file}" ]] && cd $file
}
alias h='_zshz 2>&1'


fzf-widget() {
    fd --color=always --hidden --exclude node_modules | \
    fzf \
        --prompt="$(print -Pn "${PROMPT}")" \
        --bind 'ctrl-h:change-preview-window(right,75%|hidden|right,50%)' \
        --preview-window='hidden' | open
    zle fzf-redraw-prompt
}
zle     -N    fzf-widget
bindkey '^P' fzf-widget

zmodload -i zsh/complist
bindkey -M menuselect '\e' .accept-line


fzy-downloads-widget() {
        ls --color=always -ct1 "$HOME/dl" | \
            fzy --keep-output -il 30 --prompt="$(print -Pn ${(e)fzy_prompt})" | \
            awk '{print "~/dl/" $0}' | \
            open --only-files
        zle fzf-redraw-prompt
}
zle -N fzy-downloads-widget
bindkey '^O' fzy-downloads-widget
