"""Match normalized OCR text against the configured popup trigger phrases."""

from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Iterable

DEFAULT_TRIGGER_MESSAGES = [
    {"phrase": "double kill", "event": "double_kill"},
    {"phrase": "triple kill", "event": "triple_kill"},
    {"phrase": "multikill", "event": "multikill"},
    {"phrase": "quadra kill", "event": "quadra_kill"},
    {"phrase": "stinger kill", "event": "kill", "tag": "launcher"},
    {"phrase": "penta kill", "event": "penta_kill"},
    {"phrase": "guided missile kill", "event": "kill", "tag": "missile"},
]
_EVENT_COUNTS = {
    "kill": 1,
    "double_kill": 2,
    "triple_kill": 3,
    "multikill": 4,
    "quadra_kill": 4,
    "penta_kill": 5,
}


def normalize(text: str) -> str:
    """Lowercase and collapse OCR noise (underscores, punctuation, spaces)."""
    text = (text or "").replace("_", " ")
    text = re.sub(r"[^a-z0-9 ]", " ", text.lower())
    return re.sub(r"\s+", " ", text).strip()


def _edit_distance_at_most_one(left: str, right: str) -> int:
    if abs(len(left) - len(right)) > 1:
        return 2
    if len(left) == len(right):
        mismatches = sum(a != b for a, b in zip(left, right))
        return min(mismatches, 2)

    shorter, longer = sorted((left, right), key=len)
    i = j = edits = 0
    while i < len(shorter) and j < len(longer):
        if shorter[i] == longer[j]:
            i += 1
            j += 1
        else:
            edits += 1
            j += 1
            if edits > 1:
                return 2
    if j < len(longer):
        edits += 1
    return min(edits, 2)


def _matches_phrase(text: str, phrase: str) -> bool:
    text_words = text.split()
    phrase_words = phrase.split()
    width = len(phrase_words)
    if not width or len(text_words) < width:
        return False

    for start in range(len(text_words) - width + 1):
        words = text_words[start:start + width]
        if words == phrase_words:
            return True
        edits = sum(
            _edit_distance_at_most_one(word, expected)
            for word, expected in zip(words, phrase_words)
        )
        if edits <= 1:
            return True
    return False


@dataclass(frozen=True)
class KillEvent:
    """One configured popup trigger."""

    player: str
    victim: str
    count: int
    raw: str
    event_type: str
    tag: str = ""


def parse_trigger_message(
    text: str,
    player_name: str,
    trigger_messages: Iterable[dict] | None = None,
) -> KillEvent | None:
    """Match configured popup phrases; unrelated killfeed text is ignored."""
    norm = normalize(text)
    if not norm:
        return None
    messages = DEFAULT_TRIGGER_MESSAGES if trigger_messages is None else trigger_messages
    for message in messages:
        if not isinstance(message, dict):
            continue
        phrase = normalize(str(message.get("phrase", "")))
        event_type = str(message.get("event", ""))
        count = _EVENT_COUNTS.get(event_type)
        if phrase and count and _matches_phrase(norm, phrase):
            configured_tag = message.get("tag", "")
            tag = configured_tag.strip() if isinstance(configured_tag, str) else ""
            return KillEvent(
                player=player_name,
                victim="",
                count=count,
                raw=text,
                event_type=event_type,
                tag=tag,
            )
    return None
