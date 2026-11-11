#!/usr/bin/env python3
"""Generate an offline dashboard from Animal Agent train/validate JSON artifacts."""
import argparse
import json
from pathlib import Path

AGENTS = ("baseline", "proposed")
MODES = ("train", "validate", "compare")

def read_json(path):
    if not path.is_file():
        return None
    with path.open(encoding="utf-8") as f:
        value = json.load(f)
    return value if isinstance(value, dict) else None

def read_jsonl(path):
    if not path.is_file():
        return []
    rows = []
    with path.open(encoding="utf-8") as f:
        for number, line in enumerate(f, 1):
            if line.strip():
                try:
                    value = json.loads(line)
                except json.JSONDecodeError as exc:
                    raise ValueError(str(path) + ":" + str(number) + ": " + str(exc)) from exc
                if isinstance(value, dict):
                    rows.append(value)
    return rows

def load_run(root):
    data = {"modes": {}, "missing": []}
    for mode in MODES:
        path = root / (mode + "_summary.json")
        summary = read_json(path)
        if summary is None:
            data["missing"].append(path.name)
            continue
        entry = {"summary": summary, "steps": {}}
        for agent in AGENTS:
            step_path = root / (mode + "_" + agent + "_steps.jsonl")
            entry["steps"][agent] = read_jsonl(step_path)
            if not step_path.exists():
                data["missing"].append(step_path.name)
        data["modes"][mode] = entry
    if not data["modes"]:
        raise FileNotFoundError("No train/validate/compare summaries in " + str(root))
    return data

PAGE = r"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Animal Agent Training Diagnostics</title><style>
body{margin:0;background:#f3f5f7;color:#202b35;font:14px/1.45 system-ui,sans-serif}header{background:#fff;padding:22px 4%;border-bottom:1px solid #dce2e7}
main{max-width:1400px;margin:20px auto;padding:0 20px 45px}h1{font-size:24px;margin:0}h2{font-size:17px;margin:0 0 10px}p{color:#65727d;margin:5px 0}
.controls{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:14px}label{display:flex;flex-direction:column;gap:4px;color:#65727d;font-size:12px}
select{padding:8px;border:1px solid #dce2e7;border-radius:7px;background:white}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px;margin-bottom:14px}
.panel,.kpi{background:white;border:1px solid #dce2e7;border-radius:9px;padding:14px;min-width:0}.kpis{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px;margin-bottom:14px}
.kpi small{display:block;color:#65727d;font-size:11px}.kpi strong{font-size:21px}svg{width:100%;height:260px;display:block}
.warning{background:#fff7e7;border:1px solid #e2c88d;padding:10px;border-radius:7px;margin-bottom:14px}
.tablewrap{max-height:460px;overflow:auto;border:1px solid #dce2e7;border-radius:7px}table{border-collapse:collapse;width:100%;font-size:12px;background:white}
td,th{padding:7px 8px;border-bottom:1px solid #dce2e7;text-align:left;white-space:nowrap}th{position:sticky;top:0;background:#f1f4f6}
@media(max-width:850px){.grid{grid-template-columns:1fr}.kpis{grid-template-columns:repeat(2,minmax(0,1fr))}}
</style></head><body><header><h1>Animal Agent — Training Diagnostics</h1>
<p>Baseline versus memory/affect-augmented agent. Reward and stimulus alignment are engineering measures, not perceived believability.</p></header>
<main><div class="controls">
<label>Run mode<select id="mode"></select></label><label>Agent<select id="agent"><option value="baseline">Baseline</option><option value="proposed">Proposed</option></select></label>
<label>Episode<select id="episode"></select></label><label>Filter<select id="filter"><option value="all">All decisions</option><option value="insist">INSIST opportunities</option><option value="memory">Memory present</option><option value="affect">Affect values</option><option value="energy">Energy ≤ 0.20</option><option value="override">Policy override</option></select></label></div>
<div id="warnings"></div><div class="kpis" id="kpis"></div>
<div class="grid">
<section class="panel"><h2>Reward per episode</h2><svg id="reward" viewBox="0 0 700 260"></svg><p>Total reward is affected by episode length; compare reward per decision too.</p></section>
<section class="panel"><h2>Episode length and completion</h2><svg id="length" viewBox="0 0 700 260"></svg></section>
<section class="panel"><h2>Action selection and availability</h2><svg id="actions" viewBox="0 0 700 300"></svg></section>
<section class="panel"><h2>State and state-action coverage</h2><svg id="coverage" viewBox="0 0 700 260"></svg></section>
<section class="panel"><h2>Energy trajectory</h2><svg id="energy" viewBox="0 0 700 260"></svg></section>
<section class="panel"><h2>Affect and memory trajectory</h2><svg id="affect" viewBox="0 0 700 260"></svg></section>
<section class="panel"><h2>INSIST decision funnel</h2><svg id="insist" viewBox="0 0 700 260"></svg></section>
<section class="panel"><h2>Q-table support and repetition</h2><svg id="qtable" viewBox="0 0 700 260"></svg></section>
<section class="panel"><h2>Interaction outcomes and termination</h2><svg id="interaction" viewBox="0 0 700 260"></svg></section>
<section class="panel"><h2>Stimulus alignment by scenario</h2><svg id="stimulus" viewBox="0 0 700 260"></svg><p>Heuristic compatibility only; not validated animal behaviour.</p></section></div>
<section class="panel"><h2>Episode decision inspector</h2><p id="meta"></p><div class="tablewrap"><table><thead><tr><th>Step</th><th>Context</th><th>Action</th><th>Valid actions</th><th>Q support</th><th>Q spread</th><th>Reward</th><th>Energy</th><th>Affect</th><th>Memory</th><th>Penalty</th><th>Terminal</th></tr></thead><tbody id="trace"></tbody></table></div><details><summary>Raw selected records</summary><pre id="raw"></pre></details></section>
</main><script>
var DATA=__DATA__,AGENTS=["baseline","proposed"],COL={baseline:"#356fa8",proposed:"#c47a35",green:"#39866a",grey:"#84919b"};
function el(id){return document.getElementById(id)}function num(v){return Number.isFinite(Number(v))?Number(v):0}
function esc(v){return String(v==null?"":v).replace(/[&<>"']/g,function(c){return {"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]})}
function summary(){return DATA.modes[el("mode").value].summary}function eps(a){return summary()[a].episodes||[]}function logs(a){return DATA.modes[el("mode").value].steps[a]||[]}
function selected(){var id=Number(el("episode").value);return logs(el("agent").value).filter(function(r){return Number(r.episode)===id})}
function line(id,series,key){var svg=el(id);svg.innerHTML="";var values=[];series.forEach(function(s){s.values.forEach(function(v){if(v[key]!==null&&v[key]!==undefined&&isFinite(Number(v[key])))values.push(Number(v[key]))})});
if(!values.length){svg.innerHTML='<text x="15" y="30" fill="#65727d">No compatible data.</text>';return}
var min=Math.min(0,...values),max=Math.max(1,...values),H=260,B=25,L=42;
for(var g=0;g<5;g++){var y=12+(H-B-12)*g/4;svg.innerHTML+='<line x1="'+L+'" x2="690" y1="'+y+'" y2="'+y+'" stroke="#e5e9ed"/>'}
series.forEach(function(s){var pts=s.values.map(function(v,i){if(v[key]==null)return "";var x=L+650*i/Math.max(1,s.values.length-1),y=H-B-(H-B-12)*(num(v[key])-min)/(max-min||1);return x+","+y}).filter(Boolean).join(" ");svg.innerHTML+='<polyline fill="none" stroke="'+s.color+'" stroke-width="2" points="'+pts+'"/>'})}
function bars(id,items){var svg=el(id);svg.innerHTML="";if(!items.length){svg.innerHTML='<text x="15" y="30" fill="#65727d">No data.</text>';return}
var max=Math.max(1,...items.map(function(x){return num(x.value)})),row=232/items.length;
items.forEach(function(x,i){var y=10+i*row,w=480*num(x.value)/max;svg.innerHTML+='<text x="150" y="'+(y+row*.55)+'" text-anchor="end" font-size="11" fill="#4e5b65">'+esc(x.label)+'</text><rect x="160" y="'+y+'" width="'+w+'" height="'+(row*.62)+'" rx="3" fill="'+x.color+'"/><text x="'+(166+w)+'" y="'+(y+row*.55)+'" font-size="10" fill="#65727d">'+num(x.value).toFixed(0)+'</text>'})}
function updateEpisodes(){var old=el("episode").value,ids=Array.from(new Set(logs(el("agent").value).map(function(r){return Number(r.episode)}))).sort(function(a,b){return a-b});
el("episode").innerHTML=ids.map(function(i){return '<option value="'+i+'">'+(i+1)+'</option>'}).join("");if(ids.indexOf(Number(old))>=0)el("episode").value=old}
function renderTrace(){var rows=selected(),filter=el("filter").value;var shown=rows.filter(function(r){
if(filter==="insist")return r.insist_predicate_eligible||r.insist_available||r.insist_execution_attempted;
if(filter==="memory")return num(r.memory_record_count_after_transition)>0;
if(filter==="affect")return r.affect_stress_after_transition!=null;
if(filter==="energy")return num(r.energy_before)<=.20;
if(filter==="override")return r.policy_action_before_overrides!==r.final_action_after_overrides;
return true});
el("meta").textContent=el("mode").value+" / "+el("agent").value+" / episode "+(Number(el("episode").value)+1)+" / "+shown.length+" of "+rows.length+" decisions";
el("trace").innerHTML=shown.map(function(r){return '<tr><td>'+(num(r.step)+1)+'</td><td>goal '+esc(r.goal_state)+' / prox '+esc(r.target_proximity)+'</td><td>'+esc(r.action)+'</td><td>'+esc((r.valid_actions||[]).join(", "))+'</td><td>'+(r.q_table_has_valid_action_values?"stored":"fallback")+'</td><td>'+(r.q_value_spread==null?"—":num(r.q_value_spread).toFixed(3))+'</td><td>'+num(r.reward).toFixed(3)+'</td><td>'+num(r.energy_before).toFixed(2)+' → '+num(r.energy_after).toFixed(2)+'</td><td>'+(r.affect_stress_after_transition==null?"—":"stress "+num(r.affect_stress_after_transition).toFixed(2)+" / arousal "+num(r.affect_arousal_after_transition).toFixed(2))+'</td><td>'+(r.memory_record_count_after_transition==null?"—":num(r.memory_record_count_after_transition)+" / "+num(r.memory_signal_after_transition).toFixed(2))+'</td><td>'+num(r.boredom_penalty).toFixed(2)+'</td><td>'+esc(r.terminal_reason||"")+'</td></tr>'}).join("");
el("raw").textContent=JSON.stringify(shown,null,2);
line("energy",[{color:COL.proposed,values:rows.map(function(r){return {v:r.energy_before}})},{color:COL.green,values:rows.map(function(r){return {v:r.energy_after}})}],"v");
var pr=logs("proposed").filter(function(r){return Number(r.episode)===Number(el("episode").value)});
line("affect",[{color:COL.orange,values:pr.map(function(r){return {v:r.affect_stress_after_transition}})},{color:COL.baseline,values:pr.map(function(r){return {v:r.affect_arousal_after_transition}})},{color:COL.green,values:pr.map(function(r){return {v:r.memory_signal_after_transition}})}],"v")}
function render(){var s=summary(),b=s.baseline||{},p=s.proposed||{},bc=s.coverage?.baseline||{},pc=s.coverage?.proposed||{};
var cards=[["Baseline reward/decision",num(b.mean_reward_per_step).toFixed(3)],["Proposed reward/decision",num(p.mean_reward_per_step).toFixed(3)],["Baseline decisions/episode",num(b.mean_steps_per_episode).toFixed(1)],["Proposed decisions/episode",num(p.mean_steps_per_episode).toFixed(1)],["Baseline completion",(100*num(b.completion_rate)).toFixed(1)+"%"],["Proposed completion",(100*num(p.completion_rate)).toFixed(1)+"%"],["Baseline coverage",(100*num(bc.valid_state_action_pair_coverage_on_encountered_states)).toFixed(1)+"%"],["Proposed coverage",(100*num(pc.valid_state_action_pair_coverage_on_encountered_states)).toFixed(1)+"%"]];
el("kpis").innerHTML=cards.map(function(x){return '<div class="kpi"><small>'+esc(x[0])+'</small><strong>'+esc(x[1])+'</strong></div>'}).join("");
line("reward",AGENTS.map(function(a){return {color:COL[a],values:eps(a)}}),"total_reward");
bars("length",AGENTS.flatMap(function(a){return eps(a).slice(0,50).map(function(e,i){return {label:a+" ep "+(i+1),value:e.steps,color:COL[a]}})}));
var selectedCounts={},available={};AGENTS.forEach(function(a){logs(a).forEach(function(r){var act=r.action||"unknown";selectedCounts[act]=(selectedCounts[act]||0)+1;(r.valid_actions||[]).forEach(function(v){available[v]=(available[v]||0)+1})})});
var actions=Array.from(new Set(Object.keys(selectedCounts).concat(Object.keys(available)))).sort();
bars("actions",actions.flatMap(function(a){return [{label:a+" selected",value:selectedCounts[a]||0,color:COL.proposed},{label:a+" available",value:available[a]||0,color:COL.baseline}]}));
bars("coverage",[{label:"Baseline unique states",value:bc.globally_unique_states_visited,color:COL.baseline},{label:"Proposed unique states",value:pc.globally_unique_states_visited,color:COL.proposed},{label:"Baseline state-action pairs",value:bc.globally_unique_state_action_pairs_visited,color:COL.baseline},{label:"Proposed state-action pairs",value:pc.globally_unique_state_action_pairs_visited,color:COL.proposed},{label:"Baseline available pairs",value:bc.valid_state_action_pairs_available_on_encountered_states,color:COL.baseline},{label:"Proposed available pairs",value:pc.valid_state_action_pairs_available_on_encountered_states,color:COL.proposed}]);
var d=s.training_diagnostics||{},bd=d.baseline||{},pd=d.proposed||{};
bars("insist",[{label:"Eligible",value:pd.insist_predicate_eligible_decisions,color:COL.baseline},{label:"Available",value:pd.insist_available_decisions,color:COL.green},{label:"Policy selected",value:pd.insist_policy_selections,color:COL.orange},{label:"Final selected",value:pd.insist_final_selections,color:COL.proposed},{label:"Execution attempts",value:pd.insist_execution_attempts,color:COL.grey},{label:"Meaningful responses",value:pd.insist_attempts_with_meaningful_response,color:COL.green}]);
bars("qtable",[{label:"Baseline Q-supported",value:bd.q_table_supported_decisions,color:COL.baseline},{label:"Proposed Q-supported",value:pd.q_table_supported_decisions,color:COL.proposed},{label:"Baseline fallback",value:bd.fallback_decisions,color:COL.baseline},{label:"Proposed fallback",value:pd.fallback_decisions,color:COL.orange},{label:"Repetition penalty triggers",value:pd.repetition_penalty_trigger_count,color:COL.green}]);
var endItems=[];AGENTS.forEach(function(a){var reasons={};eps(a).forEach(function(e){var k=e.terminal_reason||"not recorded";reasons[k]=(reasons[k]||0)+1});Object.keys(reasons).forEach(function(k){endItems.push({label:a+" "+k,value:reasons[k],color:COL[a]})})});
bars("interaction",endItems);
var stim=[];AGENTS.forEach(function(a){var audit=s[a]?.stimulus_response_audit_by_type||{};Object.keys(audit).forEach(function(k){stim.push({label:a+" "+k,value:100*num(audit[k].alignment_rate),color:COL[a]})})});
bars("stimulus",stim);updateEpisodes();renderTrace()}
Object.keys(DATA.modes).forEach(function(m){el("mode").add(new Option(m,m))});
if(DATA.missing.length)el("warnings").innerHTML='<div class="warning">Missing artifacts: '+DATA.missing.map(esc).join(", ")+'. Some charts may be incomplete.</div>';
el("mode").addEventListener("change",render);el("agent").addEventListener("change",function(){updateEpisodes();renderTrace()});
el("episode").addEventListener("change",renderTrace);el("filter").addEventListener("change",renderTrace);render();
</script></body></html>"""

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, default=Path("artifacts/training"))
    parser.add_argument("--output", type=Path, default=Path("artifacts/visualizations/training_dashboard.html"))
    args = parser.parse_args()
    payload = load_run(args.input_dir)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    embedded = json.dumps(payload, separators=(",", ":"), ensure_ascii=False).replace("</", "<\\/")
    args.output.write_text(PAGE.replace("__DATA__", embedded), encoding="utf-8")
    print("Dashboard saved:", args.output)
    print("Run modes:", ", ".join(payload["modes"]))
    print("Missing optional artifacts:", len(payload["missing"]))

if __name__ == "__main__":
    main()
