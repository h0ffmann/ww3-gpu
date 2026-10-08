-- cronograma.lua — draw the proposal's schedule table as the month grid a Brazilian cronograma is
-- read as: one row per activity, one column per month, a bar in the activity's month and a diamond
-- for a milestone (issue #37). The Markdown keeps the two-column Activity | Deadline table, which
-- GitHub shows and the reviewer and proposal_lint read, so the grid is never typed by hand and cannot
-- drift from it. A table is the schedule when every body row's last cell holds an MM/YYYY date; a row
-- whose text says "a partir de" or "from" is a milestone. LaTeX gets a booktabs tabular; Word and
-- any other format get a pandoc table with ■ and ◆.
local MONTHS = {
  pt = { "jan", "fev", "mar", "abr", "maio", "jun", "jul", "ago", "set", "out", "nov", "dez" },
  en = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" },
}
local LEGEND = {
  pt = { bar = "mês da atividade", mile = "marco, a partir do mês indicado" },
  en = { bar = "month of the activity", mile = "milestone, from the month shown" },
}
local lang = "en"

local function month_of(cell)
  local m, y = pandoc.utils.stringify(cell.contents):match("(%d%d)/(%d%d%d%d)")
  if m then return tonumber(y) * 12 + tonumber(m) - 1 end
end

local function schedule(tbl)
  if #tbl.bodies ~= 1 or #tbl.head.rows ~= 1 then return nil end
  local rows = {}
  for _, row in ipairs(tbl.bodies[1].body) do
    local cells = row.cells
    local month = #cells >= 2 and month_of(cells[#cells])
    if not month then return nil end
    local text = pandoc.utils.stringify(cells[1].contents) .. " " .. pandoc.utils.stringify(cells[#cells].contents)
    local mile = text:find("a partir de", 1, true) or text:lower():find("%f[%a]from%f[%A]") ~= nil
    rows[#rows + 1] = { activity = cells[1].contents, month = month, milestone = mile and true or false }
  end
  if #rows == 0 then return nil end
  return rows, tbl.head.rows[1].cells[1].contents
end

local function label(m)
  return MONTHS[lang][m % 12 + 1] .. "/" .. string.format("%02d", (m // 12) % 100)
end

local function latex(blocks)
  return (pandoc.write(pandoc.Pandoc(blocks), "latex"):gsub("%s+$", ""))
end

local function to_latex(tbl, rows, first, lo, hi)
  local n = hi - lo + 1
  -- the month columns are fixed; the activity column takes what is left of the line
  local spec = string.format("@{}>{\\raggedright\\arraybackslash}p{\\dimexpr\\linewidth-%d\\dimexpr 0.7cm+2\\tabcolsep\\relax\\relax}"
    .. "*{%d}{>{\\centering\\arraybackslash}p{0.7cm}}@{}", n, n)
  -- a year row over the month row, as a printed cronograma has it
  local years, rules, months, m = { "" }, {}, { "\\textbf{" .. latex(first) .. "}" }, lo
  while m <= hi do
    local last = math.min(hi, (m // 12) * 12 + 11)
    local col = m - lo + 2
    years[#years + 1] = string.format("\\multicolumn{%d}{c}{%d}", last - m + 1, m // 12)
    rules[#rules + 1] = string.format("\\cmidrule(lr){%d-%d}", col, col + last - m)
    m = last + 1
  end
  for k = lo, hi do months[#months + 1] = MONTHS[lang][k % 12 + 1] end
  local out = {
    "\\begin{center}",
    -- not a float, as mermaid.lua does for figures: the table stays under its heading
    "\\makeatletter\\def\\@captype{table}\\makeatother",
    "\\caption{" .. latex(tbl.caption.long) .. "}\\label{tab:cronograma}",
    -- the proposal is set at 1.5 line spacing (pagina.sty); a grid row reads better single-spaced
    "\\renewcommand{\\baselinestretch}{1}\\footnotesize\\setlength{\\tabcolsep}{3pt}",
    "\\begin{tabular}{" .. spec .. "}",
    "\\toprule",
    table.concat(years, " & ") .. " \\\\",
    table.concat(rules, " "),
    table.concat(months, " & ") .. " \\\\",
    "\\midrule",
  }
  for i, r in ipairs(rows) do
    local line = { latex(r.activity) }
    for k = lo, hi do
      if k ~= r.month then line[#line + 1] = ""
      elseif r.milestone then line[#line + 1] = "$\\blacklozenge$"
      else line[#line + 1] = "\\rule[-0.3ex]{\\linewidth}{1.5ex}" end
    end
    out[#out + 1] = table.concat(line, " & ") .. " \\\\" .. (i < #rows and " \\addlinespace[0.8ex]" or "")
  end
  out[#out + 1] = "\\bottomrule"
  out[#out + 1] = "\\end{tabular}\\par\\smallskip"
  out[#out + 1] = "\\rule[-0.1ex]{0.9em}{1.2ex}~" .. LEGEND[lang].bar
    .. ";\\quad $\\blacklozenge$~" .. LEGEND[lang].mile
  out[#out + 1] = "\\end{center}"
  return pandoc.RawBlock("latex", table.concat(out, "\n"))
end

local function to_table(tbl, rows, first, lo, hi)
  local md = function(blocks)
    return (pandoc.write(pandoc.Pandoc(blocks), "markdown", { wrap_text = "none" }):gsub("%s+$", ""):gsub("|", "\\|"))
  end
  local head, rule = { md(first) }, { "---" }
  for m = lo, hi do
    head[#head + 1] = label(m)
    rule[#rule + 1] = ":-:"
  end
  local lines = { "| " .. table.concat(head, " | ") .. " |", "|" .. table.concat(rule, "|") .. "|" }
  for _, r in ipairs(rows) do
    local line = { md(r.activity) }
    for m = lo, hi do line[#line + 1] = m ~= r.month and " " or (r.milestone and "◆" or "■") end
    lines[#lines + 1] = "| " .. table.concat(line, " | ") .. " |"
  end
  local out = pandoc.read(table.concat(lines, "\n"), "markdown").blocks
  out[1].caption = tbl.caption
  out[#out + 1] = pandoc.Para(pandoc.read("■ " .. LEGEND[lang].bar .. "; ◆ " .. LEGEND[lang].mile,
    "markdown").blocks[1].content)
  return out
end

local function Table(tbl)
  local rows, first = schedule(tbl)
  if not rows then return nil end
  local lo, hi = rows[1].month, rows[1].month
  for _, r in ipairs(rows) do
    lo, hi = math.min(lo, r.month), math.max(hi, r.month)
  end
  if FORMAT:match("latex") then return to_latex(tbl, rows, first, lo, hi) end
  return to_table(tbl, rows, first, lo, hi)
end

function Pandoc(doc)
  if pandoc.utils.stringify(doc.meta.lang or ""):match("^pt") then lang = "pt" end
  return doc:walk({ Table = Table })
end
