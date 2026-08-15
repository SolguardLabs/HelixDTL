# Epocas E Indices

## Dominio Temporal

Cada vault posee una epoca independiente. `advance` incrementa la epoca sin
cambiar el indice; `revalue` cambia ambos. Esto permite separar ventanas de
liquidacion de los cambios economicos del activo subyacente.

```mermaid
stateDiagram-v2
    [*] --> EpocaN
    EpocaN --> EpocaN1: advance
    EpocaN --> EpocaN1: revalue
    EpocaN1 --> EpocaN2: advance
    EpocaN1 --> EpocaN2: revalue
```

## Indices Contables

| Campo | Ambito | Uso |
| --- | --- | --- |
| `vault.share_index` | vault | depositos, suministro liquido y revalorizacion |
| `vault.previous_index` | vault | trazabilidad del ultimo cambio |
| `position.issue_index` | posicion | coste medio de shares liquidas |
| `lock.issued_index` | lock | valor contable de origen |
| `lock.redemption_index` | lock | indice aplicable a la solicitud |
| `redemption.quote_index` | redencion | evidencia congelada de la cotizacion |

```mermaid
flowchart LR
    VI["vault.share_index"] --> DEP["deposit"]
    VI --> REV["revalue"]
    DEP --> PI["position.issue_index"]
    PI --> LI["lock.issued_index"]
    LI --> RI["lock.redemption_index"]
    RI --> QI["redemption.quote_index"]
```

## Orden De Corte

Una solicitud registra `requested_epoch`. `settle` solo incluye solicitudes del
vault con una epoca de solicitud menor o igual a la epoca actual. El estado
`settled_epoch` permite auditar en que corte se materializo el pasivo.

```mermaid
sequenceDiagram
    participant C as Cuenta
    participant V as Vault epoca 4
    participant R as Redencion
    participant O as Operador
    C->>V: redeem
    V->>R: requested_epoch = 4
    O->>V: advance
    Note over V: epoca 5
    O->>V: settle
    V->>R: settled = true
    V->>R: settled_epoch = 5
```

## Reglas Operativas

- No reutilizar ids entre epocas.
- No ejecutar `revalue` con indice cero.
- Conciliar `previous_index` y `share_index` en cada cambio.
- Agrupar `settle` por vault, nunca por una epoca global.
- Conservar el snapshot anterior y posterior a cada corte.
- Revisar devengo, deficit y pasivos antes de aceptar una nueva epoca.

## Observabilidad

Los eventos incluyen epoca, cuenta, vault, importe y una nota estable. Para una
auditoria temporal, ordena primero por `serial`, no por id de entidad.
