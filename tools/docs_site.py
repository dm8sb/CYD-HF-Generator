#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Documentation pages of the GitHub Pages site (ARBEITSPLAN G9, E-54): the user manual (DE, EN),
the hardware page and the remote API rendered from the Markdown at the release tag.

  python tools/docs_site.py build --out site --version 2.1.0 --ref v2.1.0
  python tools/docs_site.py check site [--without-flasher]

tools/flasher_site.py assemble calls build() and check calls check(), so the release workflow (and
its dry run on every pull request to main) builds and checks the pages with the flasher site.

build needs markdown-it-py (CommonMark with GFM tables, pinned with hashes in
tools/requirements-ci.txt); check uses the standard library only. Rendering follows GitHub where
the documents rely on it: heading anchors in GitHub's slug style (so the in-document and cross-file
anchors work unchanged), bare https URLs as links, raw HTML (images with a width), HTML comments
dropped. Links to the published documents become links to their pages, images under docs/img/
are copied to img/ (only the ones a page uses), and links to any other file of the repository go
to GitHub at the tag or commit of the build (E-39). A link to a missing file fails the build.

check: every page present, the CSP of the flasher page without scripts (style and images only
from the site), no script, style element, style or event attribute, only the tags Markdown and
the documents' HTML produce (a placeholder like <name> outside code would vanish in the browser),
no resource from another origin, and every local link, anchor and image present.
"""
import argparse
import html
import os
import posixpath
import re
import shutil
import sys
import unicodedata
import urllib.parse
from html.parser import HTMLParser

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATE_DIR = os.path.join(ROOT, "flasher")

# (source in the repository, page in the site, navigation label)
PAGES = [
    ("USER_MANUAL_EN.md", "manual/en.html", "Manual (EN)"),
    ("USER_MANUAL_DE.md", "manual/de.html", "Handbuch (DE)"),
    ("docs/hardware.md", "hardware.html", "Hardware"),
    ("docs/REMOTE_API.md", "remote-api.html", "Remote API"),
]
LANG = {"USER_MANUAL_DE.md": "de"}
IMG_SRC_DIR = "docs/img/"
IMG_SITE_DIR = "img/"
CSS = "docs.css"
CSP = "default-src 'none'; style-src 'self'; img-src 'self' data:; base-uri 'none'; form-action 'none'"
ALLOWED_TAGS = {
    "html", "head", "meta", "title", "link", "body", "header", "nav", "main", "footer",
    "h1", "h2", "h3", "h4", "h5", "h6", "p", "a", "ul", "ol", "li", "table", "thead", "tbody",
    "tr", "th", "td", "code", "pre", "em", "strong", "s", "del", "img", "br", "hr", "blockquote",
    "span", "div", "details", "summary", "sup", "sub", "kbd",
}
SCHEME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:")
URL_RE = re.compile(r"https://[^\s<>\"]+")


class CheckError(Exception):
    pass


def fail(msg):
    raise CheckError(msg)


def github_slug(text):
    """Anchor of a heading as GitHub makes it (github-slugger): lower case, letters, digits,
    marks, '-', '_' and spaces kept, every space becomes '-' (no collapsing)."""
    kept = [c for c in text.strip().lower() if c in " -_" or unicodedata.category(c)[0] in "LNM"]
    return "".join(kept).replace(" ", "-")


# ---------------------------------------------------------------------------------------------
# build


def _heading_text(inline):
    return "".join(t.content for t in (inline.children or []) if t.type in ("text", "code_inline"))


def _rule_ids(state):
    """id attributes for h1..h6 like GitHub, duplicates numbered slug-1, slug-2 ..."""
    seen = set()
    tokens = state.tokens
    for i, tok in enumerate(tokens):
        if tok.type != "heading_open":
            continue
        base = github_slug(_heading_text(tokens[i + 1]))
        slug, n = base, 0
        while slug in seen:
            n += 1
            slug = "%s-%d" % (base, n)
        seen.add(slug)
        tok.attrSet("id", slug)


def _rule_autolink(state):
    """Bare https URLs in text become links (GFM autolinks, without linkify-it-py); trailing
    punctuation and an unbalanced closing parenthesis stay outside the link."""
    from markdown_it.token import Token
    for block in state.tokens:
        if block.type != "inline" or not block.children:
            continue
        out, depth = [], 0
        for tok in block.children:
            if tok.type == "link_open":
                depth += 1
            elif tok.type == "link_close":
                depth -= 1
            if tok.type != "text" or depth or "https://" not in tok.content:
                out.append(tok)
                continue
            pos = 0
            for m in URL_RE.finditer(tok.content):
                url = m.group(0)
                while url and (url[-1] in ".,:;!?'*_~" or (url[-1] == ")" and url.count("(") < url.count(")"))):
                    url = url[:-1]
                start, end = m.start(), m.start() + len(url)
                if start > pos:
                    t = Token("text", "", 0)
                    t.content = tok.content[pos:start]
                    out.append(t)
                a = Token("link_open", "a", 1)
                a.attrSet("href", url)
                t = Token("text", "", 0)
                t.content = url
                out.extend([a, t, Token("link_close", "a", -1)])
                pos = end
            if pos < len(tok.content):
                t = Token("text", "", 0)
                t.content = tok.content[pos:]
                out.append(t)
        block.children = out


def _rule_clean(state):
    """HTML comments are dropped (they hold notes such as the photo placeholders, which GitHub
    does not show either); table column alignment becomes a class, because the CSP allows no
    style attributes."""
    keep = []
    for tok in state.tokens:
        if tok.type == "html_block" and tok.content.strip().startswith("<!--") and tok.content.strip().endswith("-->"):
            continue
        if tok.type in ("th_open", "td_open"):
            m = re.match(r"^text-align:(left|center|right)$", tok.attrGet("style") or "")
            if m:
                tok.attrs.pop("style")
                tok.attrSet("class", "al-" + m.group(1))
        if tok.type == "inline" and tok.children:
            tok.children = [c for c in tok.children if not (c.type == "html_inline" and c.content.startswith("<!--"))]
        keep.append(tok)
    state.tokens = keep


def _renderer():
    try:
        from markdown_it import MarkdownIt
    except ImportError:
        fail("markdown-it-py missing: pip install --require-hashes -r tools/requirements-ci.txt")
    md = MarkdownIt("commonmark", {"html": True}).enable(["table", "strikethrough"])
    md.core.ruler.push("gh_autolink", _rule_autolink)
    md.core.ruler.push("gh_ids", _rule_ids)
    md.core.ruler.push("clean", _rule_clean)
    return md


def _page_of(src):
    for s, page, _ in PAGES:
        if s == src:
            return page
    return None


def _rel(target, page):
    """target (site path) relative to the directory of page (site path)."""
    return posixpath.relpath(target, posixpath.dirname(page) or ".")


class _Linker:
    """Rewrites the href/src of one page; collects the images to copy."""

    def __init__(self, src, page, repo, ref, images):
        self.src, self.page, self.repo, self.ref, self.images = src, page, repo, ref, images

    def __call__(self, url):
        raw = html.unescape(url)
        if raw.startswith("#") or SCHEME_RE.match(raw):
            return url
        path, _, frag = raw.partition("#")
        path = urllib.parse.unquote(path)
        target = posixpath.normpath(posixpath.join(posixpath.dirname(self.src), path))
        if target.startswith("../") or target == ".." or posixpath.isabs(target):
            fail("%s: link %r leaves the repository" % (self.src, raw))
        frag = ("#" + frag) if frag else ""
        page = _page_of(target)
        if page:
            return html.escape(_rel(page, self.page) + frag)
        full = os.path.join(ROOT, *target.split("/"))
        if target.startswith(IMG_SRC_DIR) and os.path.isfile(full):
            site = IMG_SITE_DIR + target[len(IMG_SRC_DIR):]
            self.images[site] = full
            return html.escape(_rel(site, self.page))
        if os.path.isfile(full):
            kind = "blob"
        elif os.path.isdir(full):
            kind = "tree"
        else:
            fail("%s: link to the missing file %s" % (self.src, target))
        return html.escape("https://github.com/%s/%s/%s/%s%s" % (
            self.repo, kind, self.ref, urllib.parse.quote(target), frag))


def _nav(page):
    links = ['<a href="%s">Flasher</a>' % _rel("index.html", page)]
    for _, p, label in PAGES:
        cur = ' aria-current="page"' if p == page else ""
        links.append('<a href="%s"%s>%s</a>' % (_rel(p, page), cur, html.escape(label)))
    return "<nav>%s</nav>" % " · ".join(links)


def _render_page(md, src, page, version, repo, ref, images):
    with open(os.path.join(ROOT, *src.split("/")), "r", encoding="utf-8") as f:
        text = f.read()
    env = {}
    tokens = md.parse(text, env)
    body = md.renderer.render(tokens, md.options, env)
    linker = _Linker(src, page, repo, ref, images)
    body = re.sub(r'(<(?:a|img)\b[^>]*?\s(?:href|src)=")([^"]*)(")',
                  lambda m: m.group(1) + linker(m.group(2)) + m.group(3), body)
    h1 = [i for i, t in enumerate(tokens) if t.type == "heading_open" and t.tag == "h1"]
    title = _heading_text(tokens[h1[0] + 1]).strip() if h1 else src
    if "CYD HF Generator" not in title:
        title += " – CYD HF Generator"
    gh = "https://github.com/%s" % repo
    return """<!doctype html>
<html lang="{lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="Content-Security-Policy" content="{csp}">
<meta name="referrer" content="no-referrer">
<title>{title}</title>
<link rel="icon" href="data:,">
<link rel="stylesheet" href="{css}">
</head>
<body>
<header>
{nav}
<p class="ver">Firmware {version} · <a href="{tree}">{repo_name} at {ref}</a></p>
</header>
<main>
{body}</main>
<footer>
<p>Rendered from <a href="{blob}">{src}</a> at {ref}. CYD HF Generator, Copyright (C) 2026 DM8SB, GPL-3.0-or-later.</p>
</footer>
</body>
</html>
""".format(
        lang=LANG.get(src, "en"), csp=CSP, title=html.escape(title), css=_rel(CSS, page),
        nav=_nav(page), version=html.escape(version), ref=html.escape(ref), repo_name=html.escape(repo),
        tree=html.escape("%s/tree/%s" % (gh, ref)), blob=html.escape("%s/blob/%s/%s" % (gh, ref, src)),
        src=html.escape(src), body=body)


def build(out, version, repo, ref):
    """Writes the pages, docs.css and the used images into out; returns the site paths."""
    if not re.match(r"^[A-Za-z0-9][A-Za-z0-9._+-]{0,63}$", ref):
        fail("ref %r: tag or commit expected" % ref)
    md = _renderer()
    images, written = {}, []
    for src, page, _ in PAGES:
        dst = os.path.join(out, *page.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "w", encoding="utf-8", newline="\n") as f:
            f.write(_render_page(md, src, page, version, repo, ref, images))
        written.append(page)
    for site, full in sorted(images.items()):
        dst = os.path.join(out, *site.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(full, dst)
        written.append(site)
    shutil.copyfile(os.path.join(TEMPLATE_DIR, CSS), os.path.join(out, CSS))
    written.append(CSS)
    size = sum(os.path.getsize(os.path.join(out, *p.split("/"))) for p in written)
    print("docs: %d pages, %d images, %d bytes (%s)" % (len(PAGES), len(images), size, ref))
    return written


# ---------------------------------------------------------------------------------------------
# check


class _Page(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.ids, self.refs, self.tags, self.csp, self.errors = set(), [], set(), None, []

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        self.tags.add(tag)
        line = self.getpos()[0]
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "meta" and (a.get("http-equiv") or "").lower() == "content-security-policy":
            self.csp = a.get("content")
        for name in a:
            if name == "style" or name.startswith("on"):
                self.errors.append("line %d: attribute %s on <%s>" % (line, name, tag))
        for name in ("href", "src", "srcset", "action", "formaction", "poster", "data"):
            if name in a:
                self.refs.append((line, tag, name, a[name] or ""))

    handle_startendtag = handle_starttag


def _parse(path):
    p = _Page()
    with open(path, "r", encoding="utf-8") as f:
        p.feed(f.read())
    p.close()
    return p


def check(site, without_flasher=False):
    """Validates the documentation pages of an assembled site; returns the number of links."""
    pages, n_links = {}, 0
    for _, page, _ in PAGES:
        full = os.path.join(site, *page.split("/"))
        if not os.path.isfile(full):
            fail("missing page %s" % page)
        pages[page] = _parse(full)
    if not os.path.isfile(os.path.join(site, CSS)):
        fail("missing %s" % CSS)
    for page, p in pages.items():
        if p.errors:
            fail("%s: %s" % (page, "; ".join(p.errors)))
        if p.csp != CSP:
            fail("%s: CSP %r, expected %r" % (page, p.csp, CSP))
        bad = sorted(p.tags - ALLOWED_TAGS)
        if bad:
            fail("%s: unexpected tags %s (a placeholder like <name> outside code?)"
                 % (page, ", ".join("<%s>" % t for t in bad)))
        if p.tags & {"script", "style", "iframe", "object", "embed", "form"}:
            fail("%s: script, style or embedded content" % page)
        for line, tag, attr, ref in p.refs:
            n_links += 1
            where = "%s line %d: <%s %s=%r>" % (page, line, tag, attr, ref)
            if tag == "a" and ref.startswith("https://"):
                continue
            if tag == "link" and attr == "href" and ref == "data:,":
                continue
            if SCHEME_RE.match(ref) or ref.startswith("//"):
                fail("%s: resource or link to another origin or scheme" % where)
            path, _, frag = ref.partition("#")
            path, frag = urllib.parse.unquote(path), urllib.parse.unquote(frag)
            target = posixpath.normpath(posixpath.join(posixpath.dirname(page), path)) if path else page
            if target.startswith("..") or posixpath.isabs(target) or "\\" in target:
                fail("%s: leaves the site" % where)
            if without_flasher and target == "index.html":
                continue
            if not os.path.isfile(os.path.join(site, *target.split("/"))):
                fail("%s: missing %s" % (where, target))
            if target.endswith(".md"):
                fail("%s: link to Markdown source" % where)
            if frag:
                if target not in pages:
                    fail("%s: anchor into a page that is not checked" % where)
                if frag not in pages[target].ids:
                    fail("%s: no anchor #%s in %s" % (where, frag, target))
    print("  docs: %d pages, %d links and images checked, all present" % (len(pages), n_links))
    return n_links


# ---------------------------------------------------------------------------------------------


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("build", help="render the documentation pages into a directory")
    p.add_argument("--out", required=True)
    p.add_argument("--version", required=True)
    p.add_argument("--ref", required=True, help="tag or commit the links to GitHub point to")
    p.add_argument("--repo", default="dm8sb/CYD-HF-Generator")
    p = sub.add_parser("check", help="validate the documentation pages of a site")
    p.add_argument("site")
    p.add_argument("--without-flasher", action="store_true", help="docs only: no index.html expected")
    args = ap.parse_args()
    try:
        if args.cmd == "build":
            build(args.out, args.version, args.repo, args.ref)
        else:
            check(args.site, args.without_flasher)
    except CheckError as e:
        print("error: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
