#!/usr/bin/env python3
import json, io, sys, html, argparse, collections

def add_boolean_argument(parser, name, default=True):
    # This emulates the behaviour of BooleanOptionalArgument in python versions < 3.9
    parser.add_argument(f'--{name}', action='store_true', default=default)
    parser.add_argument(f'--no-{name}', action='store_false', dest=name.replace('-','_'))

parser = argparse.ArgumentParser(description="Produce a graphviz dot file containing a compartment call graph from a CHERIoT linker report.")
parser.add_argument("linker_report_file")
add_boolean_argument(parser, "show-unreferenced")
add_boolean_argument(parser, "show-unexported")
add_boolean_argument(parser, "show-mmios")
add_boolean_argument(parser, "show-legend")
# add_boolean_argument(parser, "show-library")
args = parser.parse_args()

report = io.FileIO(args.linker_report_file)
report_json=json.load(report)

def escape_export_symbol(s):
    return s.replace('$','DOLLAR').replace('.','DOT')

def abbreviate(s, length=30):
    return s if len(s)<length else s[0:length-3] + "..."

def mmio_id(mmio):
    start = mmio['start']
    length = mmio['length']
    return f's{start}l{length}'

def mmio_label(mmio):
    start = mmio['start']
    length = mmio['length']
    return f'0x{start:08x} + 0x{length:x}'

exports={}
mmios_map={}
for compartment_name, compartment_details in report_json['compartments'].items():
    for export in compartment_details.get('exports',[]):
        export['compartment_name']=compartment_name
        export['referenced']=False
        exports[export['export_symbol']]=export
for compartment_name, compartment_details in report_json['compartments'].items():
    for import_details in compartment_details.get('imports',[]):
        import_kind = import_details['kind']
        if  import_kind == 'MMIO':
            mmios_map[mmio_id(import_details)] = import_details
        elif import_kind == 'SealedObject':
            pass # TODO
        elif 'export_symbol' in import_details:
            export_symbol = import_details['export_symbol']
            name = import_details.get('function', import_details['export_symbol'])
            export=exports[export_symbol]
            export['name']=name
            export['referenced']=True
        else:
            print(f"no export_symbol {import_details}")
print("digraph{\nrankdir=LR;")
for compartment_name, compartment_details in report_json['compartments'].items():
    label=f"<tr><td><b>{compartment_name}</b></td></tr>"
    for export in compartment_details.get('exports',[]):
        if (args.show_unexported or export['exported']) and (args.show_unreferenced or export['referenced']):
            sym = export['export_symbol']
            e = exports[sym]
            name = e.get('name', sym)
            label+=f'<tr><td port="{escape_export_symbol(sym)}" href="#" tooltip="{html.escape(name)}">{html.escape(abbreviate(name, 50))}</td></tr>'
    print(f'{compartment_name} [shape=box, style="rounded", margin="0.1" label=<<table border="0" cellborder="1" cellspacing="0">{label}</table>>]')
if args.show_mmios:
    mmios = list(mmios_map.values())
    mmios.sort(key=lambda i: i['start'])
    mmio_rows=[f'<tr><td align="left" port="{mmio_id(mmio)}">{mmio_label(mmio)}</td></tr>' for mmio in mmios]
    print(f'mmios [shape=plain label=<<table border="0" cellborder="1" cellspacing="0"><tr><td><b>MMIOs</b></td></tr>{"".join(mmio_rows)}</table>>]')

for compartment_name, compartment_details in report_json['compartments'].items():
    if 'imports' in compartment_details:
        for import_details in compartment_details['imports']:
            import_kind = import_details['kind']
            if import_kind in ("LibraryFunction", "CompartmentExport"):
                export_symbol = import_details['export_symbol']
                # if export_symbol in exports:
                export = exports[import_details['export_symbol']]
                # if export['kind'] == 'SealingKey':
                #     continue # XXX need special treatment due to . in name.
                # unexported
                called_compartment = export['compartment_name']
                # else:
                #     called_compartment = import_details.get("compartment_name")
                if compartment_name != called_compartment:
                    colour = {
                        "disabled":"red",
                        "enabled":"green",
                        "inherit":"blue",
                    }.get(export.get('interrupt_status', 'none'), "black")
                    style = {
                        'LibraryFunction':'dashed'
                    }.get(import_kind, 'solid')
                    print(f'{compartment_name}->{called_compartment}:{escape_export_symbol(export_symbol)} [color={colour} style={style}]')
            if import_kind == 'SealedObject':
                symbol = import_details['sealing_type']['symbol']
                export = exports[symbol]
                called_compartment = export['compartment_name']
                print(f'{compartment_name}->{called_compartment}:{escape_export_symbol(symbol)} [color=magenta]')
            if import_kind == 'MMIO' and args.show_mmios:
                print(f'{compartment_name}->mmios:{mmio_id(import_details)}')

if args.show_legend:
    print("""subgraph cluster_legend {
        label = <<b>Legend (solid=compartment call, dashed=library call)</b>>;
        node [shape = "plain"];
        key [label=<<table border="0" cellpadding="2" cellspacing="0" cellborder="0">
        <tr><td align="right" port="i1">Interrupts disabled</td></tr>
        <tr><td align="right" port="i2">Interrupts enabled</td></tr>
        <tr><td align="right" port="i3">Interrupts inherited</td></tr>
        <tr><td align="right" port="i4">MMIO</td></tr>
        </table>>]
        key2 [label=<<table border="0" cellpadding="2" cellspacing="0" cellborder="0">
        <tr><td port="i1">&nbsp;</td></tr>
        <tr><td port="i2">&nbsp;</td></tr>
        <tr><td port="i3">&nbsp;</td></tr>
        <tr><td port="i4">&nbsp;</td></tr>
        </table>>]
        key:i1:e -> key2:i1:w [color=red]
        key:i2:e -> key2:i2:w [color=green]
        key:i3:e -> key2:i3:w [color=blue]
        key:i4:e -> key2:i4:w [color=black]
    } """)

print("}")
