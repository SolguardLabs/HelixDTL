# Locks Y Redenciones

## Ciclo De Una Posicion Bloqueada

Un lock retira shares del suministro liquido y conserva origen, propietario,
vault, epoca, profundidad de transferencia e indices. Puede consumirse mediante
redenciones parciales o dividirse al transferir parte de sus shares.

```mermaid
stateDiagram-v2
    [*] --> Abierto: lock
    Abierto --> Abierto: transferencia parcial
    Abierto --> Cerrado: transferencia total
    Abierto --> Abierto: redencion parcial
    Abierto --> Cerrado: redencion total
    Cerrado --> [*]
```

El lock fuente reduce `shares`; el hijo recibe un id nuevo, `parent_lock_id`,
`transfer_depth + 1` y el mismo origen. La suma de shares de fuente e hijo debe
ser igual al saldo previo a la transferencia.

## Propiedad Y Linaje

```mermaid
flowchart TB
    P["Posicion de origen"] --> L1["Lock 1 / Alice"]
    L1 -->|"200 shares"| L2["Lock 2 / Bob"]
    L1 -->|"120 shares"| L3["Lock 3 / Carol"]
    L2 -->|"80 shares"| L4["Lock 4 / Dave"]
    L1 -.-> META["origin_position_id compartido"]
    L2 -.-> META
    L3 -.-> META
    L4 -.-> META
```

Una cuenta solo puede transferir o redimir un lock abierto del que sea
propietaria. La transferencia no altera `vault.locked_shares`; solo cambia la
distribucion interna. Cerrar un lock tampoco elimina su registro, de modo que el
linaje permanece auditable.

## Redencion Y Settlement

```mermaid
sequenceDiagram
    participant C as Cuenta
    participant L as Lock
    participant V as Vault
    participant R as Redencion
    C->>L: redeem(shares)
    L->>L: reduce shares
    L->>V: locked -= shares
    V->>V: pending += assets
    V->>R: crea claim
    Note over R: pendiente
    V->>R: settle por vault
    R->>R: settled = true
    C->>R: withdraw
    R-->>C: external_assets += assets
```

Una redencion captura shares, indices, activos y epocas. `withdraw` exige
propietario correcto, estado `settled` y ausencia de un retiro anterior.

## Ejemplo De Subdivision

Si un lock de `900` shares transfiere `300`, quedan dos posiciones: fuente con
`600` e hijo con `300`. Una redencion parcial de `100` sobre el hijo deja `200`;
el total bloqueado del vault cae a `800` y `100` pasan a `pending_shares`.

## Invariantes

- `source_after + child = source_before` en una transferencia.
- `redeemed_shares + shares` conserva el total historico del lock.
- `locked_shares` solo cambia al crear locks o solicitar redenciones.
- `pending_shares` cae al liquidar y aumenta `retired_shares`.
- una redencion retirada no puede volver a acreditarse.
- ids y linaje permanecen estables tras el cierre.
