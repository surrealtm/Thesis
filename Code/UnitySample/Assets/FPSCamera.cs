using UnityEngine;

public class FPSCamera : MonoBehaviour {
	[Range(0.1f, 9f)]
	[SerializeField] 
	public float sensitivity = 2f;
	
	[Tooltip("Limits vertical camera rotation. Prevents the flipping that happens when rotation goes above 90.")]
	[Range(0f, 90f)]
	[SerializeField] 
	public float pitch_clamp = 90f;

	[Range(1.0f, 100.0f)]
	[SerializeField]
	public float speed = 10.0f;

	private Vector2 rotation = Vector2.zero;
	private bool escaped;

	public void Start() {
		this.set_escaped(!Application.isFocused);
	}

	public void set_escaped(bool _escaped) {
		this.escaped = _escaped;

		if(this.escaped) {
			Cursor.visible = true;
			Cursor.lockState = CursorLockMode.None;
		} else {
			Cursor.visible = false;
			Cursor.lockState = CursorLockMode.Confined;
		}
	}

	public void Update() {
		if(Input.GetKeyDown(KeyCode.Escape)) this.set_escaped(!this.escaped);

		if(this.escaped) return;

		float frame_speed = 1.0f;
		if(Input.GetKey(KeyCode.LeftShift)) frame_speed = 2.0f;
		frame_speed *= Time.deltaTime;

		this.rotation.x += Input.GetAxis("Mouse X") * this.sensitivity;
		this.rotation.y += Input.GetAxis("Mouse Y") * this.sensitivity;

		this.rotation.y = Mathf.Clamp(this.rotation.y, -this.pitch_clamp, this.pitch_clamp);
		
		var xQuat = Quaternion.AngleAxis(this.rotation.x, Vector3.up);
		var yQuat = Quaternion.AngleAxis(this.rotation.y, Vector3.left);

		this.transform.localRotation = xQuat * yQuat;
	
		this.transform.position += this.transform.localRotation * new Vector3(Input.GetAxis("Horizontal"), 0, Input.GetAxis("Vertical")) * this.speed * frame_speed;
	}
}