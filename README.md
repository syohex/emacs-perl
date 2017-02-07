# emacs-perl

Embed Perl into Emacs.

## Sample Code

```lisp
(require 'perl)

(let ((ctx (perl-init)))
  (perl-do ctx "%a = (a => 1, b => [1, 2, 3]);")
  (let ((hash (perl-get ctx "%a")))
    (gethash "b" hash))) ;; => '(1 2 3)
```
