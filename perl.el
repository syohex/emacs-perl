;;; perl.el --- Run perl in Emacs -*- lexical-binding: t; -*-

;; Copyright (C) 2017 by Syohei YOSHIDA

;; Author: Syohei YOSHIDA <syohex@gmail.com>
;; URL: https://github.com/syohex/emacs-perl
;; Version: 0.01
;; Package-Requires: ((emacs "25"))

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

;;; Commentary:

;;; Code:

(require 'cl-lib)
(require 'subr-x)
(require 'perl-core)

(defun perl-init ()
  (perl-core-init))

(defun perl-do (context code)
  (cl-assert (stringp code))
  (perl-core-do context code))

(defun perl-get (context varname)
  (cl-assert (stringp varname))
  (cl-assert (string-match-p "^[#$@]" varname))
  (let ((sigil (string-to-char (substring varname 0 1)))
        (varname (substring varname 1)))
    (cl-case sigil
      (?$ (perl-core-get-sv context varname))
      (?@ (perl-core-get-av context varname))
      (otherwise (error "%c type is not support yet" sigil)))))

(provide 'perl)

;;; perl.el ends here
