"""Exercise the rendered demo modes and both export paths, then exit."""

from pathlib import Path
from tempfile import TemporaryDirectory

from PySide2.QtCore import QTimer, qVersion
from PySide2.QtWidgets import QApplication
from vtkmodules.vtkIOXML import vtkXMLPolyDataReader

from main import MainWindow, STYLE


def main():
    app = QApplication([])
    app.setStyle("Fusion")
    app.setStyleSheet(STYLE)
    print("Qt runtime: {}".format(qVersion()), flush=True)
    assert qVersion() == "5.12.12", "Run postprocess_demo\\use_qt_5_12_12.cmd before smoke testing"
    print("Creating window...", flush=True)
    window = MainWindow()
    print("Showing window...", flush=True)
    window.show()
    error = None

    def check():
        nonlocal error
        try:
            for variable in ("Pressure", "Temperature", "Velocity"):
                window.variable_combo.setCurrentIndex(list(("Pressure", "Temperature", "Velocity")).index(variable))
                for mode in ("云图", "切面", "等值面", "流线"):
                    print(f"Checking {variable} / {mode}...", flush=True)
                    window._set_mode(mode)
                    points = window.viewer.active_polydata.GetNumberOfPoints()
                    assert points > 0, f"{variable} / {mode}: no geometry"
            with TemporaryDirectory() as tmp:
                png = Path(tmp) / "view.png"
                vtp = Path(tmp) / "geometry.vtp"
                window.viewer.export_png(str(png))
                window.viewer.export_vtp(str(vtp))
                assert png.stat().st_size > 1000
                assert vtp.stat().st_size > 1000
                reader = vtkXMLPolyDataReader()
                reader.SetFileName(str(vtp))
                reader.Update()
                assert reader.GetOutput().GetNumberOfPoints() > 0
            print("PASS: 12 variable/mode combinations; PNG and VTP exports")
        except Exception as exc:
            error = exc
            print(f"FAIL: {exc}")
        finally:
            window.close()
            app.quit()

    QTimer.singleShot(500, check)
    app.exec_()
    if error:
        raise error


if __name__ == "__main__":
    main()
