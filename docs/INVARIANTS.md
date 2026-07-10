# Invariants

El modulo `src/invariants.c` calcula un reporte de consistencia que aparece en
cada snapshot JSON bajo `invariants`.

## Invariantes sanos

### Solvencia

Un snapshot sano debe cumplir:

```text
invariants.solvent = true
invariants.undercollateralized_assets = 0
totals.deficit_assets = 0
risk.insolvent_vaults = 0
```

El calculo compara reservas mas deficit contabilizado contra claims pendientes
del estado actual. Si las claims superan el backing, el reporte marca
undercollateralizacion.

### Quotes de locked shares

Para locked shares no yield bearing:

```text
redemption.quote_index <= redemption.issued_index
redemption.assets <= redemption.expected_assets
lock.redemption_index <= lock.issued_index
```

Los tests normales esperan `invariants.quotes_ok = true`. El CTF se resuelve al
producir un estado donde esta propiedad ya no se sostiene usando transiciones
validas.

### Pending

Una redencion pendiente no deberia exigir mas assets de los que el vault puede
cubrir con reservas o deficit ya contabilizado:

```text
invariants.pending_ok = true
```

Este invariant no sustituye a solvencia, pero ayuda a distinguir un quote
excesivo antes de ejecutar settlement.

## Campos derivados

- `live_share_assets`: valor actual de las shares liquidas al indice del vault.
- `locked_issue_assets`: valor de locks abiertos a su indice emitido.
- `locked_redemption_assets`: valor de locks abiertos a su indice de redencion.
- `pending_assets`: assets cotizados y aun no liquidados.
- `settled_assets`: assets liquidados historicamente por vaults.
- `claim_assets`: suma de redenciones creadas.
- `claim_gap_assets`: excedente de backing contra claims actuales.
- `undercollateralized_assets`: faltante de backing contra claims actuales.
- `refreshed_locks`: locks hijos cuyo indice de redencion fue refrescado.

## Lectura rapida del exploit

Un estado sospechoso suele tener una combinacion de:

```text
invariants.refreshed_locks > 0
invariants.quotes_ok = false
totals.excess_quote_assets > 0
```

Si luego se ejecuta settlement, el exceso puede convertirse en:

```text
risk.insolvent_vaults > 0
vaults[*].deficit_assets > 0
```

Los tests publicos no usan esta ruta como caso normal. Su funcion es dejar
estable el comportamiento esperado de usuarios honestos para que una mitigacion
pueda medirse sin romper operaciones legitimas.
