-- mermaid.lua — swap each ```mermaid fence for its committed render, so the PDF and Word builds
-- show the diagram GitHub draws instead of its source. The fence's `%% figure: <id>` names
-- pubs/figures/mermaid/<id>.pdf (LaTeX) or <id>.png (anything else) and `%% title:` becomes the
-- caption. scripts/figures.py makes the renders (`just figures`) and CI checks they are current;
-- a missing render stops the build here. The reading card's <summary> line, raw HTML that LaTeX
-- and Word would drop, becomes a bold heading so the card still reads as one under the figure.
local root = pandoc.path.directory(pandoc.path.directory(pandoc.path.directory(PANDOC_SCRIPT_FILE)))
if not pandoc.path.is_absolute(root) then
  root = pandoc.path.join({ pandoc.system.get_working_directory(), root })
end
local renders = pandoc.path.normalize(pandoc.path.join({ root, "pubs", "figures", "mermaid" }))

local function header(text, key)
  for line in text:gmatch("[^\n]+") do
    if line:sub(1, 2) ~= "%%" then break end
    local v = line:match("^%%%%%s*" .. key .. ":%s*(.-)%s*$")
    if v then return v end
  end
end

local function exists(path)
  local f = io.open(path, "rb")
  if f then f:close() return true end
  return false
end

function CodeBlock(el)
  if not el.classes:includes("mermaid") then return nil end
  local id, title = header(el.text, "figure"), header(el.text, "title")
  if not id or not title then
    error("mermaid.lua: a mermaid fence lacks '%% figure:' or '%% title:'; run scripts/figures.py check")
  end
  local latex = FORMAT:match("latex") ~= nil
  local file = pandoc.path.join({ renders, id .. (latex and ".pdf" or ".png") })
  if not exists(file) then
    error("mermaid.lua: no render for figure '" .. id .. "' (" .. file .. "); run `just figures`")
  end
  local caption = pandoc.read(title, "markdown").blocks[1].content
  if latex then
    local cap = pandoc.write(pandoc.Pandoc({ pandoc.Plain(caption) }), "latex"):gsub("%s+$", "")
    return pandoc.RawBlock("latex", table.concat({
      -- not a float: the figure stays above its reading card instead of drifting to the next page
      "\\begin{center}",
      "\\includegraphics[width=\\linewidth,height=0.8\\textheight,keepaspectratio]{" .. file .. "}",
      "\\makeatletter\\def\\@captype{figure}\\makeatother",
      "\\caption{" .. cap .. "}\\label{fig:" .. id .. "}",
      "\\end{center}" }, "\n"))
  end
  local img = pandoc.Image(caption, file, "", pandoc.Attr("fig-" .. id, {}, { width = "100%" }))
  if pandoc.Figure then
    return pandoc.Figure(pandoc.Plain({ img }), { pandoc.Plain(caption) }, pandoc.Attr("fig:" .. id))
  end
  img.title = "fig:"
  return pandoc.Para({ img })
end

function RawBlock(el)
  if el.format ~= "html" then return nil end
  local summary = el.text:match("<summary>(.-)</summary>")
  if summary then return pandoc.Para({ pandoc.Strong(pandoc.read(summary, "markdown").blocks[1].content) }) end
end
