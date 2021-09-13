from os import name
from typing import AbstractSet, Mapping, Tuple, Iterable, Optional
import collections.abc as abc
import math
import copy
import difflib
from xml.etree import ElementTree as ET

# ------------------------------------------------------------------------------
#   PriceDataItem
#
class PriceDataItem:
    def __init__(self, name: str, data: ET.Element, parent):
        self._name = name
        self._original = data
        self._updaters = [lambda x: x]
        self._parent = parent

    # ----------------
    # core 
    #     
    @property
    def name(self) -> str:
        return self._name

    def __str__(self) -> str:
        return self._name

    def __repr__(self) -> str:
        return ET.tostring(self.new_value).decode(encoding="utf-8").strip()

    @property
    def original(self) -> ET.Element:
        return self._original

    @property
    def new_value(self) -> ET.Element:
        return self._apply_updater(copy.deepcopy(self._original))

    def show(self) -> None:
        print(self.__repr__())

    def diff(self) -> None:
        comp_result = difflib.Differ().compare(
            [ET.tostring(self.original).decode(encoding="utf-8").strip()],
            [ET.tostring(self.new_value).decode(encoding="utf-8").strip()])

        comp_strs = [str(comp).strip() for comp in comp_result]
        if len(comp_strs) == 1:
            print(f"no diff  : {comp_strs[0]}")
        else:
            print(f"original : {comp_strs[0]}")
            print(f"           {comp_strs[1]}")
            print(f"new value: {comp_strs[2]}")
            print(f"           {comp_strs[3]}")

    # ----------------
    # apply/revert/save change(s)
    #     
    def _apply_updater(self, data: Optional[ET.Element] = None) -> ET.Element:
        result = data if data is not None else self._original
        for updater in self._updaters:
            result = updater(result)
        return result

    def revert(self, count: int = 1) -> None:
        for _ in range(min(count, len(self._updaters) - 1)):
            self._updaters.pop(-1)

    def revert_all(self) -> None:
        self._updaters = [lambda x: x]

    def save(self) -> None:
        self._apply_updater()
        self._parent._save()
        self.revert_all()

    # ----------------
    # change values
    #     
    def set(self, value: float, tag: str = "PX_LAST") -> "PriceDataItem":
        def value_setter(data: ET.Element) -> ET.Element:
            data.attrib["value"] = str(float(value))
            return data

        self._updaters.append(value_setter)
        return self

    def width_shift(self, value: float) -> "PriceDataItem":
        def width_shifter(data: ET.Element) -> ET.Element:
            data.attrib["value"] = str(float(data.attrib["value"]) + value)
            return data

        self._updaters.append(width_shifter)
        return self

    def rate_shift(self, value: float) -> "PriceDataItem":
        def rate_shifter(data: ET.Element) -> ET.Element:
            data.attrib["value"] = str(float(data.attrib["value"]) * (1 + value))
            return data

        self._updaters.append(rate_shifter)
        return self

    def log_shift(self, value: float) -> "PriceDataItem":
        def log_shifter(data: ET.Element) -> ET.Element:
            self._updater(data)
            data.attrib["value"] = str(float(data.attrib["value"]) * math.exp(value))
            return data
            
        self._updaters.append(log_shifter)
        return self

    def shift(self, value: float, method: str) -> "PriceDataItem":
        if method.strip().lower().startswith("w"):
            return self.width_shift(value)
        elif method.strip().lower().startswith("r"):
            return self.rate_shift(value)
        elif method.strip().lower().startswith("l"):
            return self.log_shift(value)
        else:
            raise ValueError(f"Invarid shift method '{method}' is detected. options=['w', 'r', 'l']")

    def __iadd__(self, value: float) -> "PriceDataItem":
        self.width_shift(value)
        return self

    def __isub__(self, value: float) -> "PriceDataItem":
        self.width_shift(-value)
        return self

    def __imul__(self, value: float) -> "PriceDataItem":
        self.rate_shift(value - 1)
        return self

    def __itruediv__(self, value: float) -> "PriceDataItem":
        self.rate_shift(1/value - 1)
        return self

# ------------------------------------------------------------------------------
#   PriceData
#
class PriceData(abc.Mapping):
    def __init__(self, path: str):
        self._path = path
        self._data = ET.parse(path)
        self._data_map: Mapping[str, PriceDataItem] = dict()
        self.reload()

    # ----------------
    # core 
    #     
    def diff(self, name: Optional[str] = None) -> None:
        if name is not None:
            self._data_map[name].diff()
        else:
            for name, item in self._data_map.items():
                if 1 < len(item._updaters):
                    print(f"--- {name} {'-' * (30 - len(name))}")
                    item.diff()

    def reload(self) -> "PriceData":
        self._data = ET.parse(self._path)
        self._data_map: Mapping[str, PriceDataItem] = dict()
        unregistered_unds = set(self._data_map.keys())

        for item in self._data.findall("PriceDataItem"):
            name = item.attrib["name"]
            self._data_map[name] = PriceDataItem(name, item, self)
            setattr(self, name, self._data_map[name])
            unregistered_unds -= {name}

        for unregistered_und in unregistered_unds:
            delattr(self, unregistered_und)

    def get(self, name: str) -> PriceDataItem:
        return self._data_map[name]

    def gets(self, names: Iterable[str]) -> Iterable[PriceDataItem]:
        return [self._data_map[name] for name in names]
        
    # ----------------
    # revert/save change(s)
    #     
    def revert(self, count: int = 1) -> None:
        for item in self._data_map.values():
            item.revert(count)

    def revert_all(self) -> None:
        for item in self._data_map.values():
            item.revert_all()

    def _save(self) -> None:
        self._data.write(self._path)

    def save(self, name: Optional[str] = None) -> None:
        if name is not None:
            self._data_map[name]._apply_updater()
            self._data_map[name].revert_all()
        else:
            for item in self._data_map.values():
                item._apply_updater()
                item.revert_all()
        self._save()

    # ----------------
    # change values
    #     
    def width_shift(self, value: float) -> "PriceData":
        for item in self._data_map.values():
            item.width_shift(value)
        return self

    def rate_shift(self, value: float) -> "PriceData":
        for item in self._data_map.values():
            item.rate_shift(value)
        return self

    def log_shift(self, value: float) -> "PriceData":
        for item in self._data_map.values():
            item.log_shift(value)
        return self

    def shift(self, value: float, method: str) -> "PriceDataItem":
        for item in self._data_map.values():
            item.shift(value, method)
        return self
    def __iadd__(self, value: float) -> "PriceData":
        for item in self._data_map.values():
            item.__iadd__(value)
        return self

    def __isub__(self, value: float) -> "PriceData":
        for item in self._data_map.values():
            item.__isub__(value)
        return self

    def __imul__(self, value: float) -> "PriceData":
        for item in self._data_map.values():
            item.__imul__(value)
        return self

    def __itruediv__(self, value: float) -> "PriceData":
        for item in self._data_map.values():
            item.__itruediv__(value)
        return self

    # ----------------
    # dict like behavior
    #     
    def items(self) -> AbstractSet[Tuple[str, PriceDataItem]]:
        return self._data_map.items()

    def keys(self) -> AbstractSet[str]:
        return self._data_map.keys()

    def values(self) -> AbstractSet[PriceDataItem]:
        return self._data_map.values()

    def __getitem__(self, name: str) -> PriceDataItem:
        return self._data_map[name]

    def __len__(self) -> int:
        return len(self._data_map)

    def __iter__(self) -> Iterable[str]:
        return self._data_map.__iter__()
