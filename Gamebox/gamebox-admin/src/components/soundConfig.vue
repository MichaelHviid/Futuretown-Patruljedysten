<template>
  <div class="card sound-card">
    <p class="card-title">
      <font-awesome-icon icon="fa-music" />{{ title }}
    </p>
    <div class="sound-fields">
      <div class="sound-field">
        <label class="sound-field-label">Lyd ID</label>
        <input v-model.number="localId" type="number" :min="0" :max="255" class="sound-input" />
      </div>
      <div class="sound-sep">·</div>
      <div class="sound-field">
        <label class="sound-field-label">Volume</label>
        <input v-model.number="localVol" type="number" :min="0" :max="21" class="sound-input sound-input-vol" />
      </div>
    </div>
    <div class="sound-actions">
      <button class="button-on" @click="save">Gem</button>
      <button class="test-btn test-green" @click="$emit('test', { joystick: 0, id: localId, vol: localVol })" title="Test på grønt joystick">▶ 🟢</button>
      <button class="test-btn test-red"   @click="$emit('test', { joystick: 1, id: localId, vol: localVol })" title="Test på rødt joystick">▶ 🔴</button>
    </div>
    <p class="state">Gem: ID={{ savedId }}, Vol={{ savedVol }}</p>
  </div>
</template>

<script>
export default {
  props: {
    title:  { type: String, default: 'Lyd' },
    id:     { type: Number, default: 0 },
    vol:    { type: Number, default: 15 },
  },
  emits: ['update:id', 'update:vol', 'save', 'test'],

  data() {
    return {
      localId:  this.id,
      localVol: this.vol,
      savedId:  this.id,
      savedVol: this.vol,
    }
  },

  watch: {
    id(v)  { this.localId  = v },
    vol(v) { this.localVol = v },
    localId(v)  { this.$emit('update:id',  v) },
    localVol(v) { this.$emit('update:vol', v) },
  },

  methods: {
    save() {
      this.savedId  = this.localId
      this.savedVol = this.localVol
      this.$emit('save')
    },
  },
}
</script>

<style scoped>
.sound-card { padding: 14px 18px; }

.sound-fields {
  display: flex;
  align-items: flex-end;
  gap: 4px;
  margin: 8px 0 6px;
}

.sound-field {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.sound-field-label {
  font-size: 0.65rem;
  color: #888;
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.sound-input {
  width: 70px;
  padding: 5px 8px;
  border: 1px solid #ccc;
  border-radius: 6px;
  font-size: 0.9rem;
  text-align: center;
}

.sound-input-vol {
  width: 52px;
}

.sound-sep {
  color: #bbb;
  font-size: 1.2rem;
  padding-bottom: 4px;
}

.sound-actions {
  display: flex;
  align-items: center;
  gap: 6px;
  margin-bottom: 4px;
}

.test-btn {
  border: none;
  border-radius: 6px;
  padding: 4px 9px;
  font-size: 0.78rem;
  cursor: pointer;
  font-weight: bold;
  transition: opacity 0.1s;
}
.test-btn:hover  { opacity: 0.8; }
.test-btn:active { opacity: 0.6; transform: scale(0.96); }

.test-green { background: #1e5e1e; color: #7fff7f; }
.test-red   { background: #5e1e1e; color: #ff8f8f; }
</style>
