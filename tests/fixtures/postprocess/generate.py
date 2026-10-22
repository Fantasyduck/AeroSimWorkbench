#!/usr/bin/env python3
"""Generate small, deterministic analytical postprocess fixtures (Python stdlib)."""
import argparse
import csv
import io
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
COORDS = (-1, 0, 1)
TIMES = (0, 1, 2)


def node_id(i, j, k):
    return i + 3 * j + 9 * k


def nodes():
    return [[x, y, z] for z in COORDS for y in COORDS for x in COORDS]


def mesh():
    cells = []
    for k in range(2):
        for j in range(2):
            for i in range(2):
                cells.append([node_id(i, j, k), node_id(i + 1, j, k),
                              node_id(i + 1, j + 1, k), node_id(i, j + 1, k),
                              node_id(i, j, k + 1), node_id(i + 1, j, k + 1),
                              node_id(i + 1, j + 1, k + 1), node_id(i, j + 1, k + 1)])
    faces = {}
    for axis, name in enumerate(('x', 'y', 'z')):
        for coord, side in ((0, 'minus'), (2, 'plus')):
            quads = []
            for a in range(2):
                for b in range(2):
                    uv = ((a, b), (a + 1, b), (a + 1, b + 1), (a, b + 1))
                    indices = []
                    for u, v in uv:
                        ijk = ([coord, u, v] if axis == 0 else
                               [u, coord, v] if axis == 1 else [u, v, coord])
                        indices.append(node_id(*ijk))
                    quads.append(indices)
            faces[name + '_' + side] = {'boundaryId': 2 * axis + (1 if coord == 0 else 2),
                                        'faceNodeIds': quads}
    return {'schema': 'analytical-cube-fixture-v1', 'nodeOrder': 'x-fastest',
            'coordinatesUnit': 'm', 'nodes': nodes(), 'cells': cells,
            'cellType': 'hexahedron-8', 'boundaries': faces}


def scalar_values(x, y, t):
    return (100000 + 1000 * x + 100 * t, 300 + 10 * y, x)


def frame_vtk(t):
    lines = ['# vtk DataFile Version 3.0', 'AeroSim analytical cube t={}'.format(t),
             'ASCII', 'DATASET STRUCTURED_POINTS', 'DIMENSIONS 3 3 3',
             'ORIGIN -1 -1 -1', 'SPACING 1 1 1', 'POINT_DATA 27']
    for name, slot in (('Pressure', 0), ('Temperature', 1), ('Phi', 2)):
        lines.extend(['SCALARS {} float 1'.format(name), 'LOOKUP_TABLE default'])
        lines.extend(str(scalar_values(x, y, t)[slot])
                     for x, y, z in nodes())
    lines.append('VECTORS Velocity float')
    lines.extend('1 0 0' for _ in nodes())
    return '\n'.join(lines) + '\n'


def cases():
    buf = io.StringIO(newline='')
    writer = csv.writer(buf, lineterminator='\n')
    writer.writerow(('caseId', 'alpha_deg', 'beta_deg', 'delta_deg', 'q_Pa',
                     'refArea_m2', 'refLength_m', 'Fx_N', 'Fy_N', 'Fz_N',
                     'Mx_Nm', 'My_Nm', 'Mz_Nm'))
    for cid, alpha, beta, delta in (('case-00', 0, 0, 0), ('case-01', 5, 0, 0),
                                    ('case-02', 5, 0, 10), ('case-03', 10, 2, 0)):
        writer.writerow((cid, alpha, beta, delta, 1000, 2, 0.5,
                         100 + 10 * alpha + 2 * delta, 5 * beta,
                         20 * alpha + 3 * delta, beta, 2 * alpha + delta, -beta))
    return buf.getvalue()


def curves():
    buf = io.StringIO(newline='')
    writer = csv.writer(buf, lineterminator='\n')
    writer.writerow(('seriesId', 'caseId', 'xName', 'xUnit', 'x',
                     'yName', 'yUnit', 'y'))
    for cid, alpha, beta, delta in (('case-00', 0, 0, 0), ('case-01', 5, 0, 0),
                                    ('case-02', 5, 0, 10), ('case-03', 10, 2, 0)):
        writer.writerow(('body-z-coefficient', cid, 'alpha', 'deg', alpha,
                         'Cz', '1', '{:.5f}'.format((20 * alpha + 3 * delta) / 2000.0)))
    return buf.getvalue()


def files():
    descriptor = {
        'schema': 'fixture-index-v1', 'mesh': 'mesh.json',
        'coordinateFrame': 'body-xyz',
        'fields': [
            {'name': 'Pressure', 'unit': 'Pa', 'components': 1, 'association': 'point'},
            {'name': 'Temperature', 'unit': 'K', 'components': 1, 'association': 'point'},
            {'name': 'Phi', 'unit': 'm', 'components': 1, 'association': 'point'},
            {'name': 'Velocity', 'unit': 'm/s', 'components': 3, 'association': 'point'}],
        'frames': [{'datasetId': 'cube-run-1', 'fileId': 'cube-t{:02d}'.format(t),
                    'frameOrder': t, 'timeValue': t, 'timeUnit': 's',
                    'valid': True, 'file': 'cube_t{:02d}.vtk'.format(t)}
                   for t in TIMES],
        'cases': 'cases.csv', 'curves': 'curves.csv'}
    generated = {
        'mesh.json': json.dumps(mesh(), indent=2, ensure_ascii=False) + '\n',
        'index.json': json.dumps(descriptor, indent=2, ensure_ascii=False) + '\n',
        'cases.csv': cases(), 'curves.csv': curves()}
    for t in TIMES:
        generated['cube_t{:02d}.vtk'.format(t)] = frame_vtk(t)
    return generated


def validate_values():
    m = mesh()
    assert len(m['nodes']) == 27 and len(m['cells']) == 8
    assert sorted(b['boundaryId'] for b in m['boundaries'].values()) == [1, 2, 3, 4, 5, 6]
    assert all(len(b['faceNodeIds']) == 4 for b in m['boundaries'].values())
    assert all(len(set(c)) == 8 and max(c) < 27 for c in m['cells'])
    assert sum(1 for x, y, z in m['nodes'] if x == 0) == 9
    assert scalar_values(0, 0, 2) == (100200, 300, 0)
    rows = list(csv.DictReader(io.StringIO(cases())))
    assert len(rows) == 4 and rows[2]['Fz_N'] == '130'
    curve_rows = list(csv.DictReader(io.StringIO(curves())))
    assert curve_rows[2]['y'] == '0.06500'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='verify checked-in files and analytic values')
    args = parser.parse_args()
    validate_values()
    expected = files()
    if args.check:
        wrong = [name for name, content in expected.items()
                 if not (ROOT / name).is_file() or
                 (ROOT / name).read_text(encoding='utf-8') != content]
        if wrong:
            parser.error('fixture missing or stale: {}'.format(', '.join(wrong)))
        print('postprocess fixtures OK: {} files, 27 nodes, 8 cells, 3 frames, 4 cases'.format(len(expected)))
    else:
        for name, content in expected.items():
            with (ROOT / name).open('w', encoding='utf-8', newline='\n') as output:
                output.write(content)
        print('generated {} postprocess fixture files'.format(len(expected)))


if __name__ == '__main__':
    main()
